#include <string.h>
#include <assert.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <radeon_drm.h>
#include <radeon_bo.h>
#include <radeon_bo_gem.h>
#include <radeon_cs.h>
#include <radeon_cs_gem.h>

#include "radeon_reg.h"
#include "evergreen_reg.h"
#include "evergreen_state.h"

struct drm {
	int fd;
	// for display
	drmModeCrtcPtr curr_crtc;
	drmModeConnectorPtr curr_connector;
	drmModeEncoderPtr curr_encoder;
	// for GPU
	struct radeon_bo_manager *bufmgr;
	struct radeon_cs_manager *csm;
	struct radeon_cs *cs;
};
extern struct drm drm;

// from radeon.h
#define RADEON_ALIGN(x,bytes) (((x) + ((bytes) - 1)) & ~((bytes) - 1))

#define CP_PACKET0(reg, n)						\
	(RADEON_CP_PACKET0 | ((n) << 16) | ((reg) >> 2))
#define CP_PACKET1(reg0, reg1)						\
	(RADEON_CP_PACKET1 | (((reg1) >> 2) << 11) | ((reg0) >> 2))
#define CP_PACKET2()							\
	(RADEON_CP_PACKET2)
#define CP_PACKET3(pkt, n)						\
	(RADEON_CP_PACKET3 | (pkt) | ((n) << 16))


static const uint32_t EVERGREEN_ROP[16] = {
    RADEON_ROP3_ZERO, /* GXclear        */
    RADEON_ROP3_DSa,  /* Gxand          */
    RADEON_ROP3_SDna, /* GXandReverse   */
    RADEON_ROP3_S,    /* GXcopy         */
    RADEON_ROP3_DSna, /* GXandInverted  */
    RADEON_ROP3_D,    /* GXnoop         */
    RADEON_ROP3_DSx,  /* GXxor          */
    RADEON_ROP3_DSo,  /* GXor           */
    RADEON_ROP3_DSon, /* GXnor          */
    RADEON_ROP3_DSxn, /* GXequiv        */
    RADEON_ROP3_Dn,   /* GXinvert       */
    RADEON_ROP3_SDno, /* GXorReverse    */
    RADEON_ROP3_Sn,   /* GXcopyInverted */
    RADEON_ROP3_DSno, /* GXorInverted   */
    RADEON_ROP3_DSan, /* GXnand         */
    RADEON_ROP3_ONE,  /* GXset          */
};

void
evergreen_start_3d(void)
{
    BEGIN_BATCH(3);
    PACK3(IT_CONTEXT_CONTROL, 2);
    E32(0x80000000);
    E32(0x80000000);
    END_BATCH();
}

static void
evergreen_sq_setup(sq_config_t *sq_conf)
{
    uint32_t sq_config, sq_gpr_resource_mgmt_1, sq_gpr_resource_mgmt_2, sq_gpr_resource_mgmt_3;
    uint32_t sq_thread_resource_mgmt, sq_thread_resource_mgmt_2;
    uint32_t sq_stack_resource_mgmt_1, sq_stack_resource_mgmt_2, sq_stack_resource_mgmt_3;

	sq_config = VC_ENABLE_bit;

    sq_config |= (EXPORT_SRC_C_bit |
				  (sq_conf->cs_prio << CS_PRIO_shift) |
				  (sq_conf->ls_prio << LS_PRIO_shift) |
				  (sq_conf->hs_prio << HS_PRIO_shift) |
				  (sq_conf->ps_prio << PS_PRIO_shift) |
				  (sq_conf->vs_prio << VS_PRIO_shift) |
				  (sq_conf->gs_prio << GS_PRIO_shift) |
				  (sq_conf->es_prio << ES_PRIO_shift));

    sq_gpr_resource_mgmt_1 = ((sq_conf->num_ps_gprs << NUM_PS_GPRS_shift) |
							  (sq_conf->num_vs_gprs << NUM_VS_GPRS_shift) |
							  (sq_conf->num_temp_gprs << NUM_CLAUSE_TEMP_GPRS_shift));
    sq_gpr_resource_mgmt_2 = ((sq_conf->num_gs_gprs << NUM_GS_GPRS_shift) |
							  (sq_conf->num_es_gprs << NUM_ES_GPRS_shift));
    sq_gpr_resource_mgmt_3 = ((sq_conf->num_hs_gprs << NUM_HS_GPRS_shift) |
							  (sq_conf->num_ls_gprs << NUM_LS_GPRS_shift));

    sq_thread_resource_mgmt = ((sq_conf->num_ps_threads << NUM_PS_THREADS_shift) |
							   (sq_conf->num_vs_threads << NUM_VS_THREADS_shift) |
							   (sq_conf->num_gs_threads << NUM_GS_THREADS_shift) |
							   (sq_conf->num_es_threads << NUM_ES_THREADS_shift));
    sq_thread_resource_mgmt_2 = ((sq_conf->num_hs_threads << NUM_HS_THREADS_shift) |
								 (sq_conf->num_ls_threads << NUM_LS_THREADS_shift));

    sq_stack_resource_mgmt_1 = ((sq_conf->num_ps_stack_entries << NUM_PS_STACK_ENTRIES_shift) |
								(sq_conf->num_vs_stack_entries << NUM_VS_STACK_ENTRIES_shift));

    sq_stack_resource_mgmt_2 = ((sq_conf->num_gs_stack_entries << NUM_GS_STACK_ENTRIES_shift) |
								(sq_conf->num_es_stack_entries << NUM_ES_STACK_ENTRIES_shift));

    sq_stack_resource_mgmt_3 = ((sq_conf->num_hs_stack_entries << NUM_HS_STACK_ENTRIES_shift) |
								(sq_conf->num_ls_stack_entries << NUM_LS_STACK_ENTRIES_shift));

    BEGIN_BATCH(16);
    /* disable dyn gprs */
    EREG(SQ_DYN_GPR_CNTL_PS_FLUSH_REQ, 0);
    PACK0(SQ_CONFIG, 4);
    E32(sq_config);
    E32(sq_gpr_resource_mgmt_1);
    E32(sq_gpr_resource_mgmt_2);
    E32(sq_gpr_resource_mgmt_3);
    PACK0(SQ_THREAD_RESOURCE_MGMT, 5);
    E32(sq_thread_resource_mgmt);
    E32(sq_thread_resource_mgmt_2);
    E32(sq_stack_resource_mgmt_1);
    E32(sq_stack_resource_mgmt_2);
    E32(sq_stack_resource_mgmt_3);
    END_BATCH();
}

static void
evergreen_fix_scissor_coordinates(int *x1, int *y1, int *x2, int *y2)
{
    /* all eg+ asics */
    if (*x2 == 0)
		*x1 = 1;
    if (*y2 == 0)
		*y1 = 1;
}

void
evergreen_set_screen_scissor(int x1, int y1, int x2, int y2)
{
    evergreen_fix_scissor_coordinates(&x1, &y1, &x2, &y2);

    BEGIN_BATCH(4);
    PACK0(PA_SC_SCREEN_SCISSOR_TL, 2);
    E32(((x1 << PA_SC_SCREEN_SCISSOR_TL__TL_X_shift) |
		 (y1 << PA_SC_SCREEN_SCISSOR_TL__TL_Y_shift)));
    E32(((x2 << PA_SC_SCREEN_SCISSOR_BR__BR_X_shift) |
		 (y2 << PA_SC_SCREEN_SCISSOR_BR__BR_Y_shift)));
    END_BATCH();
}

void
evergreen_set_vport_scissor(int id, int x1, int y1, int x2, int y2)
{
    evergreen_fix_scissor_coordinates(&x1, &y1, &x2, &y2);

    BEGIN_BATCH(4);
    PACK0(PA_SC_VPORT_SCISSOR_0_TL + id * PA_SC_VPORT_SCISSOR_0_TL_offset, 2);
    E32(((x1 << PA_SC_VPORT_SCISSOR_0_TL__TL_X_shift) |
		 (y1 << PA_SC_VPORT_SCISSOR_0_TL__TL_Y_shift) |
		 WINDOW_OFFSET_DISABLE_bit));
    E32(((x2 << PA_SC_VPORT_SCISSOR_0_BR__BR_X_shift) |
		 (y2 << PA_SC_VPORT_SCISSOR_0_BR__BR_Y_shift)));
    END_BATCH();
}

void
evergreen_set_generic_scissor(int x1, int y1, int x2, int y2)
{
    evergreen_fix_scissor_coordinates(&x1, &y1, &x2, &y2);

    BEGIN_BATCH(4);
    PACK0(PA_SC_GENERIC_SCISSOR_TL, 2);
    E32(((x1 << PA_SC_GENERIC_SCISSOR_TL__TL_X_shift) |
		 (y1 << PA_SC_GENERIC_SCISSOR_TL__TL_Y_shift) |
		 WINDOW_OFFSET_DISABLE_bit));
    E32(((x2 << PA_SC_GENERIC_SCISSOR_BR__BR_X_shift) |
		 (y2 << PA_SC_GENERIC_SCISSOR_TL__TL_Y_shift)));
    END_BATCH();
}

void
evergreen_set_window_scissor(int x1, int y1, int x2, int y2)
{
    evergreen_fix_scissor_coordinates(&x1, &y1, &x2, &y2);

    BEGIN_BATCH(4);
    PACK0(PA_SC_WINDOW_SCISSOR_TL, 2);
    E32(((x1 << PA_SC_WINDOW_SCISSOR_TL__TL_X_shift) |
		 (y1 << PA_SC_WINDOW_SCISSOR_TL__TL_Y_shift) |
		 WINDOW_OFFSET_DISABLE_bit));
    E32(((x2 << PA_SC_WINDOW_SCISSOR_BR__BR_X_shift) |
		 (y2 << PA_SC_WINDOW_SCISSOR_BR__BR_Y_shift)));
    END_BATCH();
}

void
evergreen_set_clip_rect(int id, int x1, int y1, int x2, int y2)
{
    BEGIN_BATCH(4);
    PACK0(PA_SC_CLIPRECT_0_TL + id * PA_SC_CLIPRECT_0_TL_offset, 2);
    E32(((x1 << PA_SC_CLIPRECT_0_TL__TL_X_shift) |
		 (y1 << PA_SC_CLIPRECT_0_TL__TL_Y_shift)));
    E32(((x2 << PA_SC_CLIPRECT_0_BR__BR_X_shift) |
		 (y2 << PA_SC_CLIPRECT_0_BR__BR_Y_shift)));
    END_BATCH();
}

void
evergreen_fs_setup(shader_config_t *fs_conf, uint32_t domain)
{
    uint32_t sq_pgm_resources;

    sq_pgm_resources = ((fs_conf->num_gprs << NUM_GPRS_shift) |
						(fs_conf->stack_size << STACK_SIZE_shift));

    if (fs_conf->dx10_clamp)
		sq_pgm_resources |= DX10_CLAMP_bit;

    BEGIN_BATCH(3 + 2);
    EREG(SQ_PGM_START_FS, fs_conf->shader_addr >> 8);
    RELOC_BATCH(fs_conf->bo, domain, 0);
    END_BATCH();

    BEGIN_BATCH(3);
    EREG(SQ_PGM_RESOURCES_FS, sq_pgm_resources);
    END_BATCH();
}

void
evergreen_set_default_state(struct radeon_bo *shader_bo)
{
    tex_resource_t tex_res;
    shader_config_t fs_conf;
    sq_config_t sq_conf;
    int i;

    memset(&tex_res, 0, sizeof(tex_resource_t));
    memset(&fs_conf, 0, sizeof(shader_config_t));

    evergreen_start_3d();

    /* SQ */
    sq_conf.ps_prio = 0;
    sq_conf.vs_prio = 1;
    sq_conf.gs_prio = 2;
    sq_conf.es_prio = 3;
    sq_conf.hs_prio = 0;
    sq_conf.ls_prio = 0;
    sq_conf.cs_prio = 0;

    // CHIP_FAMILY_CYPRESS:
	sq_conf.num_ps_gprs = 93;
	sq_conf.num_vs_gprs = 46;
	sq_conf.num_temp_gprs = 4;
	sq_conf.num_gs_gprs = 31;
	sq_conf.num_es_gprs = 31;
	sq_conf.num_hs_gprs = 23;
	sq_conf.num_ls_gprs = 23;
	sq_conf.num_ps_threads = 128;
	sq_conf.num_vs_threads = 20;
	sq_conf.num_gs_threads = 20;
	sq_conf.num_es_threads = 20;
	sq_conf.num_hs_threads = 20;
	sq_conf.num_ls_threads = 20;
	sq_conf.num_ps_stack_entries = 85;
	sq_conf.num_vs_stack_entries = 85;
	sq_conf.num_gs_stack_entries = 85;
	sq_conf.num_es_stack_entries = 85;
	sq_conf.num_hs_stack_entries = 85;
	sq_conf.num_ls_stack_entries = 85;

    evergreen_sq_setup(&sq_conf);

    BEGIN_BATCH(27);
    EREG(SQ_LDS_ALLOC_PS, 0);
    EREG(SQ_LDS_RESOURCE_MGMT, 0x10001000);
    EREG(SQ_DYN_GPR_RESOURCE_LIMIT_1, 0);

    PACK0(SQ_ESGS_RING_ITEMSIZE, 6);
    E32(0);
    E32(0);
    E32(0);
    E32(0);
    E32(0);
    E32(0);

    PACK0(SQ_GS_VERT_ITEMSIZE, 4);
    E32(0);
    E32(0);
    E32(0);
    E32(0);

    PACK0(SQ_VTX_BASE_VTX_LOC, 2);
    E32(0);
    E32(0);
    END_BATCH();

    /* DB */
    BEGIN_BATCH(3 + 2);
    EREG(DB_Z_INFO,                           0);
    RELOC_BATCH(shader_bo, RADEON_GEM_DOMAIN_VRAM, 0);
    END_BATCH();

    BEGIN_BATCH(3 + 2);
    EREG(DB_STENCIL_INFO,                     0);
    RELOC_BATCH(shader_bo, RADEON_GEM_DOMAIN_VRAM, 0);
    END_BATCH();

    BEGIN_BATCH(3 + 2);
    EREG(DB_HTILE_DATA_BASE,                    0);
    RELOC_BATCH(shader_bo, RADEON_GEM_DOMAIN_VRAM, 0);
    END_BATCH();

    BEGIN_BATCH(49);
    EREG(DB_DEPTH_CONTROL,                    0);

    PACK0(PA_SC_VPORT_ZMIN_0, 2);
    EFLOAT(0.0); // PA_SC_VPORT_ZMIN_0
    EFLOAT(1.0); // PA_SC_VPORT_ZMAX_0

    PACK0(DB_RENDER_CONTROL, 5);
    E32(STENCIL_COMPRESS_DISABLE_bit | DEPTH_COMPRESS_DISABLE_bit); // DB_RENDER_CONTROL
    E32(0); // DB_COUNT_CONTROL
    E32(0); // DB_DEPTH_VIEW
    E32(0x2a); // DB_RENDER_OVERRIDE
    E32(0); // DB_RENDER_OVERRIDE2

    PACK0(DB_STENCIL_CLEAR, 2);
    E32(0); // DB_STENCIL_CLEAR
    E32(0); // DB_DEPTH_CLEAR

    EREG(DB_ALPHA_TO_MASK,                    ((2 << ALPHA_TO_MASK_OFFSET0_shift)	|
											   (2 << ALPHA_TO_MASK_OFFSET1_shift)	|
											   (2 << ALPHA_TO_MASK_OFFSET2_shift)	|
											   (2 << ALPHA_TO_MASK_OFFSET3_shift)));

    EREG(DB_SHADER_CONTROL, ((EARLY_Z_THEN_LATE_Z << Z_ORDER_shift) |
							 DUAL_EXPORT_ENABLE_bit)); /* Only useful if no depth export */

    // SX
    EREG(SX_MISC,               0);

    // CB
    PACK0(SX_ALPHA_TEST_CONTROL, 5);
    E32(0); // SX_ALPHA_TEST_CONTROL
    E32(0x00000000); //CB_BLEND_RED
    E32(0x00000000); //CB_BLEND_GREEN
    E32(0x00000000); //CB_BLEND_BLUE
    E32(0x00000000); //CB_BLEND_ALPHA

    EREG(CB_SHADER_MASK,                      OUTPUT0_ENABLE_mask);

    // SC
    EREG(PA_SC_WINDOW_OFFSET,                 ((0 << WINDOW_X_OFFSET_shift) |
											   (0 << WINDOW_Y_OFFSET_shift)));
    EREG(PA_SC_CLIPRECT_RULE,                 CLIP_RULE_mask);
    EREG(PA_SC_EDGERULE,             0xAAAAAAAA);
    EREG(PA_SU_HARDWARE_SCREEN_OFFSET, 0);
    END_BATCH();

    /* clip boolean is set to always visible -> doesn't matter */
    for (i = 0; i < PA_SC_CLIPRECT_0_TL_num; i++)
		evergreen_set_clip_rect (i, 0, 0, 8192, 8192);

    for (i = 0; i < PA_SC_VPORT_SCISSOR_0_TL_num; i++)
		evergreen_set_vport_scissor (i, 0, 0, 8192, 8192);

    BEGIN_BATCH(57);
    PACK0(PA_SC_MODE_CNTL_0, 2);
    E32(0); // PA_SC_MODE_CNTL_0
    E32(0); // PA_SC_MODE_CNTL_1

    PACK0(PA_SC_LINE_CNTL, 16);
    E32(0); // PA_SC_LINE_CNTL
    E32(0); // PA_SC_AA_CONFIG
    E32(((X_ROUND_TO_EVEN << PA_SU_VTX_CNTL__ROUND_MODE_shift) |
		 PIX_CENTER_bit)); // PA_SU_VTX_CNTL
    EFLOAT(1.0);						// PA_CL_GB_VERT_CLIP_ADJ
    EFLOAT(1.0);						// PA_CL_GB_VERT_DISC_ADJ
    EFLOAT(1.0);						// PA_CL_GB_HORZ_CLIP_ADJ
    EFLOAT(1.0);						// PA_CL_GB_HORZ_DISC_ADJ
    E32(0); // PA_SC_AA_SAMPLE_LOCS_0
    E32(0);
    E32(0);
    E32(0);
    E32(0);
    E32(0);
    E32(0);
    E32(0); // PA_SC_AA_SAMPLE_LOCS_7
    E32(0xFFFFFFFF); // PA_SC_AA_MASK

    // CL
    PACK0(PA_CL_CLIP_CNTL, 8);
    E32(CLIP_DISABLE_bit); // PA_CL_CLIP_CNTL
    E32(FACE_bit); // PA_SU_SC_MODE_CNTL
    E32(VTX_XY_FMT_bit); // PA_CL_VTE_CNTL
    E32(0); // PA_CL_VS_OUT_CNTL
    E32(0); // PA_CL_NANINF_CNTL
    E32(0); // PA_SU_LINE_STIPPLE_CNTL
    E32(0); // PA_SU_LINE_STIPPLE_SCALE
    E32(0); // PA_SU_PRIM_FILTER_CNTL

    // SU
    PACK0(PA_SU_POLY_OFFSET_DB_FMT_CNTL, 6);
    E32(0);
    E32(0);
    E32(0);
    E32(0);
    E32(0);
    E32(0);

    /* src = semantic id 0; mask = semantic id 1 */
    EREG(SPI_VS_OUT_ID_0, ((0 << SEMANTIC_0_shift) |
						   (1 << SEMANTIC_1_shift)));
    PACK0(SPI_PS_INPUT_CNTL_0 + (0 << 2), 2);
    /* SPI_PS_INPUT_CNTL_0 maps to GPR[0] - load with semantic id 0 */
    E32(((0    << SEMANTIC_shift)	|
		 (0x01 << DEFAULT_VAL_shift)));
    /* SPI_PS_INPUT_CNTL_1 maps to GPR[1] - load with semantic id 1 */
    E32(((1    << SEMANTIC_shift)	|
		 (0x01 << DEFAULT_VAL_shift)));

    PACK0(SPI_INPUT_Z, 8);
    E32(0); // SPI_INPUT_Z
    E32(0); // SPI_FOG_CNTL
    E32(LINEAR_CENTROID_ENA__X_ON_AT_CENTROID << LINEAR_CENTROID_ENA_shift); // SPI_BARYC_CNTL
    E32(0); // SPI_PS_IN_CONTROL_2
    E32(0);
    E32(0);
    E32(0);
    E32(0);
    END_BATCH();

    // clear FS
    fs_conf.bo = shader_bo;
    evergreen_fs_setup(&fs_conf, RADEON_GEM_DOMAIN_VRAM);

    // VGT
    BEGIN_BATCH(46);

    PACK0(VGT_MAX_VTX_INDX, 4);
    E32(0xffffff);
    E32(0);
    E32(0);
    E32(0);

    PACK0(VGT_INSTANCE_STEP_RATE_0, 2);
    E32(0);
    E32(0);

    PACK0(VGT_REUSE_OFF, 2);
    E32(0);
    E32(0);

    PACK0(PA_SU_POINT_SIZE, 17);
    E32(0); // PA_SU_POINT_SIZE
    E32(0); // PA_SU_POINT_MINMAX
    E32((8 << PA_SU_LINE_CNTL__WIDTH_shift)); /* Line width 1 pixel */ // PA_SU_LINE_CNTL
    E32(0); // PA_SC_LINE_STIPPLE
    E32(0); // VGT_OUTPUT_PATH_CNTL
    E32(0); // VGT_HOS_CNTL
    E32(0);
    E32(0);
    E32(0);
    E32(0);
    E32(0);
    E32(0);
    E32(0);
    E32(0);
    E32(0);
    E32(0);
    E32(0); // VGT_GS_MODE

    EREG(VGT_PRIMITIVEID_EN,                  0);
    EREG(VGT_MULTI_PRIM_IB_RESET_EN,          0);
    EREG(VGT_SHADER_STAGES_EN,          0);

    PACK0(VGT_STRMOUT_CONFIG, 2);
    E32(0);
    E32(0);
    END_BATCH();
}

static void
evergreen_cp_set_surface_sync(uint32_t sync_type,
							  uint32_t size, uint64_t mc_addr,
							  struct radeon_bo *bo, uint32_t rdomains, uint32_t wdomain)
{
    uint32_t cp_coher_size;
    if (size == 0xffffffff)
		cp_coher_size = 0xffffffff;
    else
		cp_coher_size = ((size + 255) >> 8);

    BEGIN_BATCH(5 + 2);
    PACK3(IT_SURFACE_SYNC, 4);
    E32(sync_type);
    E32(cp_coher_size);
    E32((mc_addr >> 8));
    E32(10); /* poll interval */
    RELOC_BATCH(bo, rdomains, wdomain);
    END_BATCH();
}


void
evergreen_vs_setup(shader_config_t *vs_conf, uint32_t domain)
{
    uint32_t sq_pgm_resources, sq_pgm_resources_2;

    sq_pgm_resources = ((vs_conf->num_gprs << NUM_GPRS_shift) |
						(vs_conf->stack_size << STACK_SIZE_shift));

    if (vs_conf->dx10_clamp)
		sq_pgm_resources |= DX10_CLAMP_bit;
    if (vs_conf->uncached_first_inst)
		sq_pgm_resources |= UNCACHED_FIRST_INST_bit;

    sq_pgm_resources_2 = ((vs_conf->single_round << SINGLE_ROUND_shift) |
						  (vs_conf->double_round << DOUBLE_ROUND_shift));

    if (vs_conf->allow_sdi)
		sq_pgm_resources_2 |= ALLOW_SINGLE_DENORM_IN_bit;
    if (vs_conf->allow_sd0)
		sq_pgm_resources_2 |= ALLOW_SINGLE_DENORM_OUT_bit;
    if (vs_conf->allow_ddi)
		sq_pgm_resources_2 |= ALLOW_DOUBLE_DENORM_IN_bit;
    if (vs_conf->allow_ddo)
		sq_pgm_resources_2 |= ALLOW_DOUBLE_DENORM_OUT_bit;

    /* flush SQ cache */
    evergreen_cp_set_surface_sync(SH_ACTION_ENA_bit,
								  vs_conf->shader_size, vs_conf->shader_addr,
								  vs_conf->bo, domain, 0);

    BEGIN_BATCH(3 + 2);
    EREG(SQ_PGM_START_VS, vs_conf->shader_addr >> 8);
    RELOC_BATCH(vs_conf->bo, domain, 0);
    END_BATCH();

    BEGIN_BATCH(4);
    PACK0(SQ_PGM_RESOURCES_VS, 2);
    E32(sq_pgm_resources);
    E32(sq_pgm_resources_2);
    END_BATCH();
}

void
evergreen_ps_setup(shader_config_t *ps_conf, uint32_t domain)
{
    uint32_t sq_pgm_resources, sq_pgm_resources_2;

    sq_pgm_resources = ((ps_conf->num_gprs << NUM_GPRS_shift) |
						(ps_conf->stack_size << STACK_SIZE_shift));

    if (ps_conf->dx10_clamp)
		sq_pgm_resources |= DX10_CLAMP_bit;
    if (ps_conf->uncached_first_inst)
		sq_pgm_resources |= UNCACHED_FIRST_INST_bit;
    if (ps_conf->clamp_consts)
		sq_pgm_resources |= CLAMP_CONSTS_bit;

    sq_pgm_resources_2 = ((ps_conf->single_round << SINGLE_ROUND_shift) |
						  (ps_conf->double_round << DOUBLE_ROUND_shift));

    if (ps_conf->allow_sdi)
		sq_pgm_resources_2 |= ALLOW_SINGLE_DENORM_IN_bit;
    if (ps_conf->allow_sd0)
		sq_pgm_resources_2 |= ALLOW_SINGLE_DENORM_OUT_bit;
    if (ps_conf->allow_ddi)
		sq_pgm_resources_2 |= ALLOW_DOUBLE_DENORM_IN_bit;
    if (ps_conf->allow_ddo)
		sq_pgm_resources_2 |= ALLOW_DOUBLE_DENORM_OUT_bit;

    /* flush SQ cache */
    evergreen_cp_set_surface_sync(SH_ACTION_ENA_bit,
								  ps_conf->shader_size, ps_conf->shader_addr,
								  ps_conf->bo, domain, 0);

    BEGIN_BATCH(3 + 2);
    EREG(SQ_PGM_START_PS, ps_conf->shader_addr >> 8);
    RELOC_BATCH(ps_conf->bo, domain, 0);
    END_BATCH();

    BEGIN_BATCH(5);
    PACK0(SQ_PGM_RESOURCES_PS, 3);
    E32(sq_pgm_resources);
    E32(sq_pgm_resources_2);
    E32(ps_conf->export_mode);
    END_BATCH();
}

void
evergreen_set_render_target(cb_config_t *cb_conf, uint32_t domain)
{
    uint32_t cb_color_info, cb_color_attrib = 0, cb_color_dim;
    int pitch, slice, h;

    cb_color_info = ((cb_conf->endian      << ENDIAN_shift)				|
					 (cb_conf->format      << CB_COLOR0_INFO__FORMAT_shift)		|
					 (cb_conf->array_mode  << CB_COLOR0_INFO__ARRAY_MODE_shift)		|
					 (cb_conf->number_type << NUMBER_TYPE_shift)			|
					 (cb_conf->comp_swap   << COMP_SWAP_shift)				|
					 (cb_conf->source_format << SOURCE_FORMAT_shift)                    |
					 (cb_conf->resource_type << RESOURCE_TYPE_shift));
    if (cb_conf->blend_clamp)
		cb_color_info |= BLEND_CLAMP_bit;
    if (cb_conf->fast_clear)
		cb_color_info |= FAST_CLEAR_bit;
    if (cb_conf->compression)
		cb_color_info |= COMPRESSION_bit;
    if (cb_conf->blend_bypass)
		cb_color_info |= BLEND_BYPASS_bit;
    if (cb_conf->simple_float)
		cb_color_info |= SIMPLE_FLOAT_bit;
    if (cb_conf->round_mode)
		cb_color_info |= CB_COLOR0_INFO__ROUND_MODE_bit;
    if (cb_conf->tile_compact)
		cb_color_info |= CB_COLOR0_INFO__TILE_COMPACT_bit;
    if (cb_conf->rat)
		cb_color_info |= RAT_bit;

    /* bit 4 needs to be set for linear and depth/stencil surfaces */
    if (cb_conf->non_disp_tiling)
		cb_color_attrib |= CB_COLOR0_ATTRIB__NON_DISP_TILING_ORDER_bit;

    pitch = (cb_conf->w / 8) - 1;
    h = RADEON_ALIGN(cb_conf->h, 8);
    slice = ((cb_conf->w * h) / 64) - 1;

    switch (cb_conf->resource_type) {
    case BUFFER:
		/* number of elements in the surface */
		cb_color_dim = pitch * slice;
		break;
    default:
		/* w/h of the surface */
		cb_color_dim = (((cb_conf->w - 1) << WIDTH_MAX_shift) |
						((cb_conf->h - 1) << HEIGHT_MAX_shift));
		break;
    }

    BEGIN_BATCH(3 + 2);
    EREG(CB_COLOR0_BASE + (0x3c * cb_conf->id), (cb_conf->base >> 8));
    RELOC_BATCH(cb_conf->bo, 0, domain);
    END_BATCH();

    /* Set CMASK & FMASK buffer to the offset of color buffer as
     * we don't use those this shouldn't cause any issue and we
     * then have a valid cmd stream
     */
    BEGIN_BATCH(3 + 2);
    EREG(CB_COLOR0_CMASK + (0x3c * cb_conf->id), (0     >> 8));
    RELOC_BATCH(cb_conf->bo, 0, domain);
    END_BATCH();
    BEGIN_BATCH(3 + 2);
    EREG(CB_COLOR0_FMASK + (0x3c * cb_conf->id), (0     >> 8));
    RELOC_BATCH(cb_conf->bo, 0, domain);
    END_BATCH();

    /* tiling config */
    BEGIN_BATCH(3 + 2);
    EREG(CB_COLOR0_ATTRIB + (0x3c * cb_conf->id), cb_color_attrib);
    RELOC_BATCH(cb_conf->bo, 0, domain);
    END_BATCH();
    BEGIN_BATCH(3 + 2);
    EREG(CB_COLOR0_INFO + (0x3c * cb_conf->id), cb_color_info);
    RELOC_BATCH(cb_conf->bo, 0, domain);
    END_BATCH();

    BEGIN_BATCH(33);
    EREG(CB_COLOR0_PITCH + (0x3c * cb_conf->id), pitch);
    EREG(CB_COLOR0_SLICE + (0x3c * cb_conf->id), slice);
    EREG(CB_COLOR0_VIEW + (0x3c * cb_conf->id), 0);
    EREG(CB_COLOR0_DIM + (0x3c * cb_conf->id), cb_color_dim);
    EREG(CB_COLOR0_CMASK_SLICE + (0x3c * cb_conf->id), 0);
    EREG(CB_COLOR0_FMASK_SLICE + (0x3c * cb_conf->id), 0);
    PACK0(CB_COLOR0_CLEAR_WORD0 + (0x3c * cb_conf->id), 4);
    E32(0);
    E32(0);
    E32(0);
    E32(0);
    EREG(CB_TARGET_MASK, (cb_conf->pmask << TARGET0_ENABLE_shift));
    EREG(CB_COLOR_CONTROL, (EVERGREEN_ROP[cb_conf->rop] |
							(CB_NORMAL << CB_COLOR_CONTROL__MODE_shift)));
    EREG(CB_BLEND0_CONTROL, cb_conf->blendcntl);
    END_BATCH();
}

void
evergreen_set_spi(int vs_export_count, int num_interp)
{
    BEGIN_BATCH(8);
    /* Interpolator setup */
    EREG(SPI_VS_OUT_CONFIG, (vs_export_count << VS_EXPORT_COUNT_shift));
    PACK0(SPI_PS_IN_CONTROL_0, 3);
    E32(((num_interp << NUM_INTERP_shift) |
		 LINEAR_GRADIENT_ENA_bit)); // SPI_PS_IN_CONTROL_0
    E32(0); // SPI_PS_IN_CONTROL_1
    E32(0); // SPI_INTERP_CONTROL_0
    END_BATCH();
}

void
evergreen_set_alu_consts(const_config_t *const_conf, uint32_t domain)
{
    /* size reg is units of 16 consts (4 dwords each) */
    uint32_t size = const_conf->size_bytes >> 8;

    if (size == 0)
		size = 1;

    /* flush SQ cache */
    evergreen_cp_set_surface_sync(SH_ACTION_ENA_bit,
								  const_conf->size_bytes, const_conf->const_addr,
								  const_conf->bo, domain, 0);

    switch (const_conf->type) {
    case SHADER_TYPE_VS:
		BEGIN_BATCH(3);
		EREG(SQ_ALU_CONST_BUFFER_SIZE_VS_0, size);
		END_BATCH();
		BEGIN_BATCH(3 + 2);
		EREG(SQ_ALU_CONST_CACHE_VS_0, const_conf->const_addr >> 8);
		RELOC_BATCH(const_conf->bo, domain, 0);
		END_BATCH();
		break;
    case SHADER_TYPE_PS:
		BEGIN_BATCH(3);
		EREG(SQ_ALU_CONST_BUFFER_SIZE_PS_0, size);
		END_BATCH();
		BEGIN_BATCH(3 + 2);
		EREG(SQ_ALU_CONST_CACHE_PS_0, const_conf->const_addr >> 8);
		RELOC_BATCH(const_conf->bo, domain, 0);
		END_BATCH();
		break;
    default:
		/* error */
		break;
    }

}

static void
evergreen_set_vtx_resource(vtx_resource_t *res, uint32_t domain, int offset)
{
    uint32_t sq_vtx_constant_word2, sq_vtx_constant_word3, sq_vtx_constant_word4;

    sq_vtx_constant_word2 = ((((res->vb_addr) >> 32) & BASE_ADDRESS_HI_mask) |
							 ((res->vtx_size_dw << 2) << SQ_VTX_CONSTANT_WORD2_0__STRIDE_shift) |
							 (res->format << SQ_VTX_CONSTANT_WORD2_0__DATA_FORMAT_shift) |
							 (res->num_format_all << SQ_VTX_CONSTANT_WORD2_0__NUM_FORMAT_ALL_shift) |
							 (res->endian << SQ_VTX_CONSTANT_WORD2_0__ENDIAN_SWAP_shift));
    if (res->clamp_x)
	    sq_vtx_constant_word2 |= SQ_VTX_CONSTANT_WORD2_0__CLAMP_X_bit;

    if (res->format_comp_all)
	    sq_vtx_constant_word2 |= SQ_VTX_CONSTANT_WORD2_0__FORMAT_COMP_ALL_bit;

    if (res->srf_mode_all)
	    sq_vtx_constant_word2 |= SQ_VTX_CONSTANT_WORD2_0__SRF_MODE_ALL_bit;

    sq_vtx_constant_word3 = ((res->dst_sel_x << SQ_VTX_CONSTANT_WORD3_0__DST_SEL_X_shift) |
							 (res->dst_sel_y << SQ_VTX_CONSTANT_WORD3_0__DST_SEL_Y_shift) |
							 (res->dst_sel_z << SQ_VTX_CONSTANT_WORD3_0__DST_SEL_Z_shift) |
							 (res->dst_sel_w << SQ_VTX_CONSTANT_WORD3_0__DST_SEL_W_shift));

    if (res->uncached)
		sq_vtx_constant_word3 |= SQ_VTX_CONSTANT_WORD3_0__UNCACHED_bit;

    /* XXX ??? */
    sq_vtx_constant_word4 = 0;

	evergreen_cp_set_surface_sync(VC_ACTION_ENA_bit, offset, 0, res->bo, domain, 0);

    BEGIN_BATCH(10 + 2);
    PACK0(SQ_FETCH_RESOURCE + res->id * SQ_FETCH_RESOURCE_offset, 8);
    E32(res->vb_addr & 0xffffffff);				// 0: BASE_ADDRESS
    E32((res->vtx_num_entries << 2) - 1);			// 1: SIZE
    E32(sq_vtx_constant_word2);	// 2: BASE_HI, STRIDE, CLAMP, FORMAT, ENDIAN
    E32(sq_vtx_constant_word3);		// 3: swizzles
    E32(sq_vtx_constant_word4);		// 4: num elements
    E32(0);							// 5: n/a
    E32(0);							// 6: n/a
    E32(SQ_TEX_VTX_VALID_BUFFER << SQ_VTX_CONSTANT_WORD7_0__TYPE_shift);	// 7: TYPE
    RELOC_BATCH(res->bo, domain, 0);
    END_BATCH();
}

void
evergreen_draw_auto(draw_config_t *draw_conf)
{
    BEGIN_BATCH(10);
    EREG(VGT_PRIMITIVE_TYPE, draw_conf->prim_type);
    PACK3(IT_INDEX_TYPE, 1);
    E32(draw_conf->index_type);
    PACK3(IT_NUM_INSTANCES, 1);
    E32(draw_conf->num_instances);
    PACK3(IT_DRAW_INDEX_AUTO, 2);
    E32(draw_conf->num_indices);
    E32(draw_conf->vgt_draw_initiator);
    END_BATCH();
}

void
evergreen_finish_op(struct radeon_bo *dst, int size, struct radeon_bo *vbo, int offset, int vtx_size)
{
    draw_config_t   draw_conf;
    vtx_resource_t  vtx_res;

    CLEAR (draw_conf);
    CLEAR (vtx_res);

    /* Vertex buffer setup */
    vtx_res.id              = SQ_FETCH_RESOURCE_vs;
    vtx_res.vtx_size_dw     = vtx_size / 4;
    vtx_res.vtx_num_entries = offset / 4;
    vtx_res.vb_addr         = 0;
    vtx_res.bo              = vbo;
    vtx_res.dst_sel_x       = SQ_SEL_X;
    vtx_res.dst_sel_y       = SQ_SEL_Y;
    vtx_res.dst_sel_z       = SQ_SEL_Z;
    vtx_res.dst_sel_w       = SQ_SEL_W;
    evergreen_set_vtx_resource(&vtx_res, RADEON_GEM_DOMAIN_GTT, offset);

    /* Draw */
    draw_conf.prim_type          = DI_PT_RECTLIST;
    draw_conf.vgt_draw_initiator = DI_SRC_SEL_AUTO_INDEX;
    draw_conf.num_instances      = 1;
    draw_conf.num_indices        = vtx_res.vtx_num_entries / vtx_res.vtx_size_dw;
    draw_conf.index_type         = DI_INDEX_SIZE_16_BIT;

    evergreen_draw_auto(&draw_conf);

    /* sync dst surface */
    evergreen_cp_set_surface_sync((CB_ACTION_ENA_bit | CB0_DEST_BASE_ENA_bit),
								  size, 0, dst, 0, RADEON_GEM_DOMAIN_VRAM);
}

