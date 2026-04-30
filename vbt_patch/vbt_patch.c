#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <getopt.h>

#define _INTEL_BIOS_PRIVATE
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#define __packed __attribute__((packed))

#include "intel_vbt_defs.h"

#define MAX_MIPI_PANEL MAX_MIPI_CONFIGURATIONS

struct patch_opts {
	int clock;
	int hz;
	int rotation;
	int panel_on;
	int panel_off;
	int bl_on;
	int bl_off;
	int cycle;
	const char *input;
	const char *output;
};

struct vbt_ctx {
	uint8_t *data;
	size_t size;
	struct vbt_header *vbt;
	const struct bdb_header *bdb;
	size_t bdb_size;
	int panel;
	const struct bdb_generic_dtd *gdtd;
	const struct bdb_mipi_config *mipi_cfg;
};

static const char *rotation_str[] = { "0°", "90°", "180°", "270°" };

static const void *find_block(const struct bdb_header *bdb, size_t bdb_size,
			      int block_id)
{
	const uint8_t *base = (const uint8_t *)bdb;
	size_t index = bdb->header_size;
	uint32_t total = bdb->bdb_size;

	if (total > bdb_size) {
		total = bdb_size;
	}

	while (index + 3 < total) {
		uint8_t id = base[index];
		uint32_t size;

		if (id == BDB_MIPI_SEQUENCE && base[index + 3] >= 3) {
			size = *(uint32_t *)(base + index + 4);
		} else {
			size = *(uint16_t *)(base + index + 1);
		}

		index += 3;
		if (id == block_id) {
			return base + index;
		}
		index += size;
	}

	return NULL;
}

static double dtd_hz(const struct generic_dtd_entry *dtd)
{
	uint32_t htotal = dtd->hactive + dtd->hblank;
	uint32_t vtotal = dtd->vactive + dtd->vblank;

	if (!htotal || !vtotal) {
		return 0;
	}

	return (double)dtd->pixel_clock * 1000.0 / (htotal * vtotal);
}

static void update_checksum(struct vbt_header *vbt)
{
	uint8_t *ptr = (uint8_t *)vbt;
	uint8_t sum = 0;

	vbt->vbt_checksum = 0;
	for (size_t i = 0; i < vbt->vbt_size; i++) {
		sum += ptr[i];
	}
	vbt->vbt_checksum = 0x100 - sum;
}

static uint8_t *read_file(const char *path, size_t *out_size)
{
	struct stat st;
	int fd = open(path, O_RDONLY);

	if (fd < 0) {
		perror("open input");
		return NULL;
	}

	if (fstat(fd, &st) < 0) {
		perror("fstat");
		close(fd);
		return NULL;
	}

	uint8_t *data = malloc(st.st_size);
	if (!data || read(fd, data, st.st_size) != st.st_size) {
		perror("read");
		free(data);
		close(fd);
		return NULL;
	}

	close(fd);
	*out_size = st.st_size;
	return data;
}

static int write_file(const char *path, const uint8_t *data, size_t size)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	if (fd < 0) {
		perror("open output");
		return -1;
	}

	if (write(fd, data, size) != (ssize_t)size) {
		perror("write");
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

static struct vbt_header *find_vbt(uint8_t *data, size_t size)
{
	for (size_t i = 0; i + 4 <= size; i++) {
		if (!memcmp(data + i, "$VBT", 4)) {
			return (struct vbt_header *)(data + i);
		}
	}

	return NULL;
}

static void print_dtd(const struct vbt_ctx *ctx)
{
	const struct generic_dtd_entry *dtd = &ctx->gdtd->dtd[ctx->panel];

	printf("[Block 58] Generic DTD:\n");
	printf("  hdisplay: %u, vdisplay: %u\n", dtd->hactive, dtd->vactive);
	printf("  htotal: %u, vtotal: %u\n",
	       dtd->hactive + dtd->hblank, dtd->vactive + dtd->vblank);
	printf("  pixel_clock: %u kHz\n", dtd->pixel_clock);
	printf("  refresh: %.2f Hz\n", dtd_hz(dtd));
}

static void print_mipi(const struct vbt_ctx *ctx)
{
	const struct mipi_config *cfg = &ctx->mipi_cfg->config[ctx->panel];
	const struct mipi_pps_data *pps = &ctx->mipi_cfg->pps[ctx->panel];

	printf("\n[Block 52] MIPI config:\n");
	printf("  lanes: %u\n", cfg->lane_cnt + 1);
	printf("  rotation: %s (%u)\n", rotation_str[cfg->rotation], cfg->rotation);

	printf("\n[Block 52] MIPI power sequences:\n");
	printf("  panel_on:    %.1f ms\n", pps->panel_on_delay / 10.0);
	printf("  bl_enable:   %.1f ms\n", pps->bl_enable_delay / 10.0);
	printf("  bl_disable:  %.1f ms\n", pps->bl_disable_delay / 10.0);
	printf("  panel_off:   %.1f ms\n", pps->panel_off_delay / 10.0);
	printf("  power_cycle: %.1f ms\n", pps->panel_power_cycle_delay / 10.0);
}

static void print_info(const struct vbt_ctx *ctx)
{
	printf("Panel index: %d\n\n", ctx->panel);

	if (ctx->gdtd) {
		print_dtd(ctx);
	}
	if (ctx->mipi_cfg && ctx->panel < MAX_MIPI_PANEL) {
		print_mipi(ctx);
	}
}

static int resolve_hz_to_clock(const struct vbt_ctx *ctx, int hz)
{
	const struct generic_dtd_entry *dtd = &ctx->gdtd->dtd[ctx->panel];
	uint32_t htotal = dtd->hactive + dtd->hblank;
	uint32_t vtotal = dtd->vactive + dtd->vblank;

	return (int)((uint64_t)hz * htotal * vtotal / 1000);
}

static int patch_clock(const struct vbt_ctx *ctx, int clock)
{
	struct generic_dtd_entry *dtd =
		(struct generic_dtd_entry *)&ctx->gdtd->dtd[ctx->panel];
	double old_hz = dtd_hz(dtd);
	uint32_t old_clock = dtd->pixel_clock;

	dtd->pixel_clock = clock;
	printf("Patching pixel_clock: %u → %u kHz (%.2f → %.2f Hz)\n",
	       old_clock, (uint32_t)clock, old_hz, dtd_hz(dtd));
	return 1;
}

static int patch_rotation(const struct vbt_ctx *ctx, int rotation)
{
	struct mipi_config *cfg =
		(struct mipi_config *)&ctx->mipi_cfg->config[ctx->panel];
	int old = cfg->rotation;

	cfg->rotation = rotation;
	printf("Patching MIPI rotation: %s → %s\n",
	       rotation_str[old], rotation_str[rotation]);
	return 1;
}

static int patch_pps_field(void *field, const char *name, int ms)
{
	uint16_t old, val;

	memcpy(&old, field, sizeof(old));
	val = ms * 10;
	memcpy(field, &val, sizeof(val));
	printf("Patching MIPI %s: %.1f ms → %.1f ms\n",
	       name, old / 10.0, val / 10.0);
	return 1;
}

static int apply_patches(struct vbt_ctx *ctx, struct patch_opts *opts)
{
	int changed = 0;

	if (opts->hz > 0 && ctx->gdtd) {
		opts->clock = resolve_hz_to_clock(ctx, opts->hz);
		printf("--hz %d → clock %d kHz\n", opts->hz, opts->clock);
	}

	if (opts->clock > 0 && ctx->gdtd) {
		changed += patch_clock(ctx, opts->clock);
	}

	if (!ctx->mipi_cfg || ctx->panel >= MAX_MIPI_PANEL) {
		return changed;
	}

	if (opts->rotation >= 0) {
		changed += patch_rotation(ctx, opts->rotation);
	}

	struct mipi_pps_data *pps =
		(struct mipi_pps_data *)&ctx->mipi_cfg->pps[ctx->panel];

	if (opts->panel_on >= 0) {
		changed += patch_pps_field(&pps->panel_on_delay, "panel_on", opts->panel_on);
	}
	if (opts->panel_off >= 0) {
		changed += patch_pps_field(&pps->panel_off_delay, "panel_off", opts->panel_off);
	}
	if (opts->bl_on >= 0) {
		changed += patch_pps_field(&pps->bl_enable_delay, "bl_enable", opts->bl_on);
	}
	if (opts->bl_off >= 0) {
		changed += patch_pps_field(&pps->bl_disable_delay, "bl_disable", opts->bl_off);
	}
	if (opts->cycle >= 0) {
		changed += patch_pps_field(&pps->panel_power_cycle_delay, "power_cycle", opts->cycle);
	}

	return changed;
}

static int init_ctx(struct vbt_ctx *ctx)
{
	ctx->vbt = find_vbt(ctx->data, ctx->size);
	if (!ctx->vbt) {
		fprintf(stderr, "VBT signature not found\n");
		return -1;
	}

	size_t vbt_off = (uint8_t *)ctx->vbt - ctx->data;

	ctx->bdb = (const struct bdb_header *)
		(ctx->data + vbt_off + ctx->vbt->bdb_offset);
	ctx->bdb_size = ctx->size - vbt_off - ctx->vbt->bdb_offset;

	const struct bdb_lfp_options *lfp_opts =
		find_block(ctx->bdb, ctx->bdb_size, BDB_LFP_OPTIONS);
	if (!lfp_opts) {
		fprintf(stderr, "Block 40 (LFP options) not found\n");
		return -1;
	}
	ctx->panel = lfp_opts->panel_type;

	ctx->gdtd = find_block(ctx->bdb, ctx->bdb_size, BDB_GENERIC_DTD);
	ctx->mipi_cfg = find_block(ctx->bdb, ctx->bdb_size, BDB_MIPI_CONFIG);

	if (!ctx->gdtd && !ctx->mipi_cfg) {
		fprintf(stderr, "Neither Block 58 (DTD) nor Block 52 (MIPI) found\n");
		return -1;
	}

	return 0;
}

static struct patch_opts parse_args(int argc, char **argv)
{
	struct patch_opts opts = {
		.clock = -1, .hz = -1, .rotation = -1,
		.panel_on = -1, .panel_off = -1,
		.bl_on = -1, .bl_off = -1, .cycle = -1,
		.input = NULL, .output = NULL,
	};

	static struct option long_opts[] = {
		{"clock",     required_argument, NULL, 'c'},
		{"hz",        required_argument, NULL, 'f'},
		{"rotation",  required_argument, NULL, 'r'},
		{"panel-on",  required_argument, NULL, 'O'},
		{"panel-off", required_argument, NULL, 'F'},
		{"bl-on",     required_argument, NULL, 'B'},
		{"bl-off",    required_argument, NULL, 'b'},
		{"cycle",     required_argument, NULL, 'C'},
		{NULL, 0, NULL, 0}
	};

	if (argc < 2) {
		return opts;
	}

	opts.input = argv[1];
	optind = 2;

	int ch;
	while ((ch = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
		switch (ch) {
		case 'c': opts.clock = atoi(optarg); break;
		case 'f': opts.hz = atoi(optarg); break;
		case 'r': opts.rotation = atoi(optarg); break;
		case 'O': opts.panel_on = atoi(optarg); break;
		case 'F': opts.panel_off = atoi(optarg); break;
		case 'B': opts.bl_on = atoi(optarg); break;
		case 'b': opts.bl_off = atoi(optarg); break;
		case 'C': opts.cycle = atoi(optarg); break;
		default: opts.input = NULL; return opts;
		}
	}

	if (optind < argc) {
		opts.output = argv[optind];
	}

	return opts;
}

static int has_patches(const struct patch_opts *opts)
{
	return opts->clock > 0 || opts->hz > 0 || opts->rotation >= 0 ||
	       opts->panel_on >= 0 || opts->panel_off >= 0 ||
	       opts->bl_on >= 0 || opts->bl_off >= 0 || opts->cycle >= 0;
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s <input_vbt> [options] [output_vbt]\n"
		"\n"
		"With no options or output file, prints current VBT state.\n"
		"\n"
		"DTD options (Block 58):\n"
		"  --clock <kHz>       Set pixel clock (determines refresh rate)\n"
		"  --hz <rate>         Set refresh rate (calculates clock automatically)\n"
		"\n"
		"MIPI options (Block 52):\n"
		"  --rotation <0-3>    Panel rotation (0=0°, 1=90°, 2=180°, 3=270°)\n"
		"  --panel-on <ms>     Panel power-on delay\n"
		"  --panel-off <ms>    Panel power-off delay\n"
		"  --bl-on <ms>        Backlight enable delay\n"
		"  --bl-off <ms>       Backlight disable delay\n"
		"  --cycle <ms>        Power cycle delay\n",
		prog);
}

int main(int argc, char **argv)
{
	struct patch_opts opts = parse_args(argc, argv);

	if (!opts.input) {
		usage(argv[0]);
		return 1;
	}

	struct vbt_ctx ctx = {0};
	ctx.data = read_file(opts.input, &ctx.size);
	if (!ctx.data) {
		return 1;
	}

	if (init_ctx(&ctx) < 0) {
		free(ctx.data);
		return 1;
	}

	if (!has_patches(&opts)) {
		print_info(&ctx);
		free(ctx.data);
		return 0;
	}

	if (!opts.output) {
		fprintf(stderr, "Output file required when patching\n");
		free(ctx.data);
		return 1;
	}

	int changed = apply_patches(&ctx, &opts);
	if (changed == 0) {
		printf("No changes to write.\n");
		free(ctx.data);
		return 0;
	}

	update_checksum(ctx.vbt);
	if (write_file(opts.output, ctx.data, ctx.size) < 0) {
		free(ctx.data);
		return 1;
	}

	printf("Wrote: %s\n", opts.output);
	free(ctx.data);
	return 0;
}
