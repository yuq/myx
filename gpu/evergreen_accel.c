#include <string.h>

#include "evergreen_state.h"

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
    RELOC_BATCH(shaders_bo, RADEON_GEM_DOMAIN_VRAM, 0);
    END_BATCH();

    BEGIN_BATCH(3 + 2);
    EREG(DB_STENCIL_INFO,                     0);
    RELOC_BATCH(shaders_bo, RADEON_GEM_DOMAIN_VRAM, 0);
    END_BATCH();

    BEGIN_BATCH(3 + 2);
    EREG(DB_HTILE_DATA_BASE,                    0);
    RELOC_BATCH(shaders_bo, RADEON_GEM_DOMAIN_VRAM, 0);
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
		evergreen_set_clip_rect (pScrn, i, 0, 0, 8192, 8192);

    for (i = 0; i < PA_SC_VPORT_SCISSOR_0_TL_num; i++)
		evergreen_set_vport_scissor (pScrn, i, 0, 0, 8192, 8192);

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
    fs_conf.bo = shaders_bo;
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
