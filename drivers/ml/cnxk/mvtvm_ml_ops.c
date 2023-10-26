/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include "cnxk_ml_dev.h"
#include "cnxk_ml_model.h"
#include "cnxk_ml_ops.h"

/* ML model macros */
#define MVTVM_ML_MODEL_MEMZONE_NAME "ml_mvtvm_model_mz"

int
mvtvm_ml_dev_configure(struct cnxk_ml_dev *cnxk_mldev, const struct rte_ml_dev_config *conf)
{
	int ret;

	RTE_SET_USED(conf);

	/* Configure TVMDP library */
	ret = tvmdp_configure(cnxk_mldev->mldev->data->nb_models, rte_get_tsc_cycles);
	if (ret != 0)
		plt_err("TVMDP configuration failed, error = %d\n", ret);

	return ret;
}

int
mvtvm_ml_dev_close(struct cnxk_ml_dev *cnxk_mldev)
{
	int ret;

	RTE_SET_USED(cnxk_mldev);

	/* Close TVMDP library configuration */
	ret = tvmdp_close();
	if (ret != 0)
		plt_err("TVMDP close failed, error = %d\n", ret);

	return ret;
}

int
mvtvm_ml_model_load(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_model_params *params,
		    struct cnxk_ml_model *model)
{
	struct mvtvm_ml_model_object object[ML_MVTVM_MODEL_OBJECT_MAX];
	struct tvmrt_glow_callback *callback;
	char str[RTE_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;
	size_t model_object_size = 0;
	uint16_t nb_mrvl_layers;
	uint16_t nb_llvm_layers;
	uint8_t layer_id = 0;
	uint64_t mz_size = 0;
	int ret;

	RTE_SET_USED(cnxk_mldev);

	ret = mvtvm_ml_model_blob_parse(params, object);
	if (ret != 0)
		return ret;

	model_object_size = RTE_ALIGN_CEIL(object[0].size, RTE_CACHE_LINE_MIN_SIZE) +
			    RTE_ALIGN_CEIL(object[1].size, RTE_CACHE_LINE_MIN_SIZE) +
			    RTE_ALIGN_CEIL(object[2].size, RTE_CACHE_LINE_MIN_SIZE);
	mz_size += model_object_size;

	/* Allocate memzone for model object */
	snprintf(str, RTE_MEMZONE_NAMESIZE, "%s_%u", MVTVM_ML_MODEL_MEMZONE_NAME, model->model_id);
	mz = plt_memzone_reserve_aligned(str, mz_size, 0, ML_CN10K_ALIGN_SIZE);
	if (!mz) {
		plt_err("plt_memzone_reserve failed : %s", str);
		return -ENOMEM;
	}

	/* Copy mod.so */
	model->mvtvm.object.so.addr = mz->addr;
	model->mvtvm.object.so.size = object[0].size;
	rte_memcpy(model->mvtvm.object.so.name, object[0].name, TVMDP_NAME_STRLEN);
	rte_memcpy(model->mvtvm.object.so.addr, object[0].buffer, object[0].size);
	rte_free(object[0].buffer);

	/* Copy mod.json */
	model->mvtvm.object.json.addr =
		RTE_PTR_ADD(model->mvtvm.object.so.addr,
			    RTE_ALIGN_CEIL(model->mvtvm.object.so.size, RTE_CACHE_LINE_MIN_SIZE));
	model->mvtvm.object.json.size = object[1].size;
	rte_memcpy(model->mvtvm.object.json.name, object[1].name, TVMDP_NAME_STRLEN);
	rte_memcpy(model->mvtvm.object.json.addr, object[1].buffer, object[1].size);
	rte_free(object[1].buffer);

	/* Copy mod.params */
	model->mvtvm.object.params.addr =
		RTE_PTR_ADD(model->mvtvm.object.json.addr,
			    RTE_ALIGN_CEIL(model->mvtvm.object.json.size, RTE_CACHE_LINE_MIN_SIZE));
	model->mvtvm.object.params.size = object[2].size;
	rte_memcpy(model->mvtvm.object.params.name, object[2].name, TVMDP_NAME_STRLEN);
	rte_memcpy(model->mvtvm.object.params.addr, object[2].buffer, object[2].size);
	rte_free(object[2].buffer);

	/* Get metadata - stage 1 */
	ret = tvmdp_model_metadata_get_stage1(model->mvtvm.object.json.addr,
					      model->mvtvm.object.json.size,
					      &model->mvtvm.metadata);
	if (ret != 0) {
		plt_err("TVMDP: Failed to parse metadata - stage 1, model_id = %u, error = %d",
			model->model_id, ret);
		goto error;
	}

	/* Set model fields */
	plt_strlcpy(model->name, model->mvtvm.metadata.model.name, TVMDP_NAME_STRLEN);
	model->batch_size = 1;
	model->nb_layers = model->mvtvm.metadata.model.nb_layers;

	/* Update layer info */
	nb_mrvl_layers = 0;
	nb_llvm_layers = 0;
	for (layer_id = 0; layer_id < model->mvtvm.metadata.model.nb_layers; layer_id++) {
		rte_strscpy(model->layer[layer_id].name,
			    model->mvtvm.metadata.model.layer[layer_id].name, TVMDP_NAME_STRLEN);
		if (strcmp(model->mvtvm.metadata.model.layer[layer_id].type, "mrvl") == 0 ||
		    strcmp(model->mvtvm.metadata.model.layer[layer_id].type, "MRVL") == 0) {
			model->layer[layer_id].type = ML_CNXK_LAYER_TYPE_MRVL;
			nb_mrvl_layers++;
		} else if (strcmp(model->mvtvm.metadata.model.layer[layer_id].type, "llvm") == 0 ||
			   strcmp(model->mvtvm.metadata.model.layer[layer_id].type, "LLVM") == 0) {
			model->layer[layer_id].type = ML_CNXK_LAYER_TYPE_LLVM;
			nb_llvm_layers++;
		}
	}

	if ((nb_llvm_layers == 0) && (nb_mrvl_layers == 0)) {
		plt_err("Invalid model, nb_llvm_layers = %u, nb_mrvl_layers = %u", nb_llvm_layers,
			nb_mrvl_layers);
		goto error;
	}

	/* Set model subtype */
	if ((nb_llvm_layers == 0) && (nb_mrvl_layers == 1))
		model->subtype = ML_CNXK_MODEL_SUBTYPE_TVM_MRVL;
	else if ((nb_llvm_layers > 0) && (nb_mrvl_layers == 0))
		model->subtype = ML_CNXK_MODEL_SUBTYPE_TVM_LLVM;
	else
		model->subtype = ML_CNXK_MODEL_SUBTYPE_TVM_HYBRID;

	/* Set callback function array */
	if (model->subtype != ML_CNXK_MODEL_SUBTYPE_TVM_LLVM) {
		callback = &model->mvtvm.cb;
		callback->tvmrt_glow_layer_load = cn10k_ml_layer_load;
		callback->tvmrt_glow_layer_unload = cn10k_ml_layer_unload;
	} else {
		callback = NULL;
	}

	/* Initialize model in TVMDP */
	ret = tvmdp_model_load(cnxk_mldev, model->model_id, (void *)(&model->mvtvm.object),
			       callback);
	if (ret != 0) {
		plt_err("TVMDP: Model load failed, model_id = %u, error = %d", model->model_id,
			ret);
		goto error;
	}

	/* Get model metadata - stage 2 */
	ret = tvmdp_model_metadata_get_stage2(model->model_id, &model->mvtvm.metadata);
	if (ret != 0) {
		plt_err("TVMDP: Failed to get metadata, model_id = %u, error = %d\n",
			model->model_id, ret);
		goto error;
	}

	/* Update model I/O data */
	mvtvm_ml_model_io_info_set(model);

	/* Set model info */
	mvtvm_ml_model_info_set(cnxk_mldev, model);

	return 0;

error:
	rte_memzone_free(mz);

	return ret;
}

int
mvtvm_ml_model_unload(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model)
{
	char str[RTE_MEMZONE_NAMESIZE];
	const struct plt_memzone *mz;
	int ret;

	RTE_SET_USED(cnxk_mldev);

	/* Initialize model in TVMDP */
	ret = tvmdp_model_unload(model->model_id);
	if (ret != 0) {
		plt_err("TVMDP: Model unload failed, model_id = %u, error = %d", model->model_id,
			ret);
		return ret;
	}

	snprintf(str, RTE_MEMZONE_NAMESIZE, "%s_%u", MVTVM_ML_MODEL_MEMZONE_NAME, model->model_id);
	mz = rte_memzone_lookup(str);
	if (mz == NULL) {
		plt_err("Memzone lookup failed for TVM model: model_id = %u, mz = %s",
			model->model_id, str);
		return -EINVAL;
	}

	return plt_memzone_free(mz);
}

int
mvtvm_ml_model_start(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model)
{
	struct cnxk_ml_layer *layer;

	uint16_t layer_id = 0;
	int ret = 0;

next_layer:
	layer = &model->layer[layer_id];
	if (layer->type == ML_CNXK_LAYER_TYPE_MRVL) {
		ret = cn10k_ml_layer_start(cnxk_mldev, model->model_id, layer->name);
		if (ret != 0) {
			plt_err("Layer start failed, model_id = %u, layer_name = %s, error = %d",
				model->model_id, layer->name, ret);
			return ret;
		}
	}
	layer_id++;

	if (layer_id < model->nb_layers)
		goto next_layer;

	return 0;
}

int
mvtvm_ml_model_stop(struct cnxk_ml_dev *cnxk_mldev, struct cnxk_ml_model *model)
{
	struct cnxk_ml_layer *layer;

	uint16_t layer_id = 0;
	int ret = 0;

next_layer:
	layer = &model->layer[layer_id];
	if (layer->type == ML_CNXK_LAYER_TYPE_MRVL) {
		ret = cn10k_ml_layer_stop(cnxk_mldev, model->model_id, layer->name);
		if (ret != 0) {
			plt_err("Layer stop failed, model_id = %u, layer_name = %s, error = %d",
				model->model_id, layer->name, ret);
			return ret;
		}
	}
	layer_id++;

	if (layer_id < model->nb_layers)
		goto next_layer;

	return 0;
}