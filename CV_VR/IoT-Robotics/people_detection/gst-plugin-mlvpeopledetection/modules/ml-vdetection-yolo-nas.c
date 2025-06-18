/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *     * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 * GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <math.h>

#include <gst/utils/common-utils.h>
#include <gst/utils/batch-utils.h>
#include <gst/ml/ml-module-utils.h>
#include <gst/ml/ml-module-video-detection.h>

// Set the default debug category.
#define GST_CAT_DEFAULT gst_ml_module_debug

#define GST_ML_SUB_MODULE_CAST(obj) ((GstMLSubModule*)(obj))

// Output dimensions depends on input[w, h] and n_classes.
// Dimensions format: <<1, D, n_classes>, <1, D, 4>>
// D = w/32 * h/32 + w/16 * h/16 + w/8 * h/8
// MODULE_CAPS support input dim [32, 32] -> [1920, 1088]. Number class 1 -> 1001
#define GST_ML_MODULE_CAPS \
    "neural-network/tensors, " \
    "type = (string) { INT8, UINT8, FLOAT32 }, " \
    "dimensions = (int) < <1, [21, 42840], 4>, <1, [21, 42840]>, <1, [21, 42840]> >; " \
    "neural-network/tensors, " \
    "type = (string) { INT8, UINT8, FLOAT32 }, " \
    "dimensions = (int) < <1, [21, 42840], 2>, <1, [21, 42840], 2>, <1, [21, 42840], 81> >; " \
    "neural-network/tensors, " \
    "type = (string) { INT8, UINT8, FLOAT32 }, " \
    "dimensions = (int) < <1, 4, [21, 42840]>, <1, [1, 1001], [21, 42840]> >; " \
    "neural-network/tensors, " \
    "type = (string) { INT8, UINT8, FLOAT32 }, " \
    "dimensions = (int) < <1, [5, 1005], [21, 42840]> >; " \
    "neural-network/tensors, " \
    "type = (string) { UINT8, FLOAT32 }, " \
    "dimensions = (int) < <1, [21, 42840], [1, 1001]>, <1, [21, 42840], 4> >; " \
    "neural-network/tensors, " \
    "type = (string) { UINT8, FLOAT32 }, " \
    "dimensions = (int) < <1, [21, 42840], 4>, <1, [21, 42840], [1,1001]> >; "

// Module caps instance
static GstStaticCaps modulecaps = GST_STATIC_CAPS (GST_ML_MODULE_CAPS);

typedef struct _GstMLSubModule GstMLSubModule;

struct _GstMLSubModule {
  // Configurated ML capabilities in structure format.
  GstMLInfo  mlinfo;

  // List of prediction labels.
  GHashTable *labels;
  // Confidence threshold value.
  gfloat     threshold;

  // Offset values for each of the tensors for dequantization of some tensors.
  gdouble    qoffsets[GST_ML_MAX_TENSORS];
  // Scale values for each of the tensors for dequantization of some tensors.
  gdouble    qscales[GST_ML_MAX_TENSORS];
};

gpointer
gst_ml_module_open (void)
{
  GstMLSubModule *submodule = NULL;
  guint idx = 0;

  submodule = g_slice_new0 (GstMLSubModule);
  g_return_val_if_fail (submodule != NULL, NULL);

  // Initialize the quantization offsets and scales.
  for (idx = 0; idx < GST_ML_MAX_TENSORS; idx++) {
    submodule->qoffsets[idx] = 0.0;
    submodule->qscales[idx] = 1.0;
  }

  return (gpointer) submodule;
}

void
gst_ml_module_close (gpointer instance)
{
  GstMLSubModule *submodule = GST_ML_SUB_MODULE_CAST (instance);

  if (NULL == submodule)
    return;

  if (submodule->labels != NULL)
    g_hash_table_destroy (submodule->labels);

  g_slice_free (GstMLSubModule, submodule);
}

GstCaps *
gst_ml_module_caps (void)
{
  static GstCaps *caps = NULL;
  static gsize inited = 0;

  if (g_once_init_enter (&inited)) {
    caps = gst_static_caps_get (&modulecaps);
    g_once_init_leave (&inited, 1);
  }

  return caps;
}

gboolean
gst_ml_module_configure (gpointer instance, GstStructure * settings)
{
  GstMLSubModule *submodule = GST_ML_SUB_MODULE_CAST (instance);
  GstCaps *caps = NULL, *mlcaps = NULL;
  const gchar *input = NULL;
  GValue list = G_VALUE_INIT;
  gdouble threshold = 0.0;
  gboolean success = FALSE;

  g_return_val_if_fail (submodule != NULL, FALSE);
  g_return_val_if_fail (settings != NULL, FALSE);

  if (!(success = gst_structure_has_field (settings, GST_ML_MODULE_OPT_CAPS))) {
    GST_ERROR ("Settings stucture does not contain configuration caps!");
    goto cleanup;
  }

  // Fetch the configuration capabilities.
  gst_structure_get (settings, GST_ML_MODULE_OPT_CAPS, GST_TYPE_CAPS, &caps, NULL);
  // Get the set of supported capabilities.
  mlcaps = gst_ml_module_caps ();

  // Make sure that the configuration capabilities are fixated and supported.
  if (!(success = gst_caps_is_fixed (caps))) {
    GST_ERROR ("Configuration caps are not fixated!");
    goto cleanup;
  } else if (!(success = gst_caps_can_intersect (caps, mlcaps))) {
    GST_ERROR ("Configuration caps are not supported!");
    goto cleanup;
  }

  if (!(success = gst_ml_info_from_caps (&(submodule->mlinfo), caps))) {
    GST_ERROR ("Failed to get ML info from confguration caps!");
    goto cleanup;
  }

  input = gst_structure_get_string (settings, GST_ML_MODULE_OPT_LABELS);

  // Parse funtion will print error message if it fails, simply goto cleanup.
  if (!(success = gst_ml_parse_labels (input, &list)))
    goto cleanup;

  submodule->labels = gst_ml_load_labels (&list);

  // Labels funtion will print error message if it fails, simply goto cleanup.
  if (!(success = (submodule->labels != NULL)))
    goto cleanup;

  success = gst_structure_has_field (settings, GST_ML_MODULE_OPT_THRESHOLD);
  if (!success) {
    GST_ERROR ("Settings stucture does not contain threshold value!");
    goto cleanup;
  }

  gst_structure_get_double (settings, GST_ML_MODULE_OPT_THRESHOLD, &threshold);
  submodule->threshold = threshold / 100.0;

  if ((GST_ML_INFO_TYPE (&(submodule->mlinfo)) == GST_ML_TYPE_INT8) ||
      (GST_ML_INFO_TYPE (&(submodule->mlinfo)) == GST_ML_TYPE_UINT8)) {
    GstStructure *constants = NULL;
    const GValue *qoffsets = NULL, *qscales = NULL;
    guint idx = 0, n_tensors = 0;

    success = gst_structure_has_field (settings, GST_ML_MODULE_OPT_CONSTANTS);
    if (!success) {
      GST_ERROR ("Settings stucture does not contain constants value!");
      goto cleanup;
    }

    constants = GST_STRUCTURE (g_value_get_boxed (
        gst_structure_get_value (settings, GST_ML_MODULE_OPT_CONSTANTS)));

    if (!(success = gst_structure_has_field (constants, "q-offsets"))) {
      GST_ERROR ("Missing quantization offsets coefficients!");
      goto cleanup;
    } else if (!(success = gst_structure_has_field (constants, "q-scales"))) {
      GST_ERROR ("Missing quantization scales coefficients!");
      goto cleanup;
    }

    qoffsets = gst_structure_get_value (constants, "q-offsets");
    qscales = gst_structure_get_value (constants, "q-scales");
    n_tensors = GST_ML_INFO_N_TENSORS (&(submodule->mlinfo));

    if (!(success = (gst_value_array_get_size (qoffsets) == n_tensors))) {
      GST_ERROR ("Expecting %u dequantization offsets entries but received "
          "only %u!", n_tensors, gst_value_array_get_size (qoffsets));
      goto cleanup;
    } else if (!(success = (gst_value_array_get_size (qscales) == n_tensors))) {
      GST_ERROR ("Expecting %u dequantization scales entries but received "
          "only %u!", n_tensors, gst_value_array_get_size (qscales));
      goto cleanup;
    }

    for (idx = 0; idx < n_tensors; idx++) {
      submodule->qoffsets[idx] =
          g_value_get_double (gst_value_array_get_value (qoffsets, idx));
      submodule->qscales[idx] =
          g_value_get_double (gst_value_array_get_value (qscales, idx));
    }
  }

cleanup:
  if (caps != NULL)
    gst_caps_unref (caps);

  g_value_unset (&list);
  gst_structure_free (settings);

  return success;
}

static void
gst_ml_module_parse_tripleblock_frame (GstMLSubModule * submodule,
    GArray * predictions, GstMLFrame * mlframe)
{
  GstProtectionMeta *pmeta = NULL;
  GstMLBoxPrediction *prediction = NULL;
  GstMLLabel *label = NULL;
  gpointer bboxes = NULL, scores = NULL, classes = NULL;
  GstVideoRectangle region = { 0, };
  GstMLType mltype = GST_ML_TYPE_UNKNOWN;
  guint n_paxels = 0, idx = 0, class_idx = 0;
  gfloat confidence = 0;
  gint nms = -1;

  pmeta = gst_buffer_get_protection_meta_id (mlframe->buffer,
      gst_batch_channel_name (0));

  prediction = &(g_array_index (predictions, GstMLBoxPrediction, 0));
  prediction->info = pmeta->info;

  // Extract the source tensor region with actual data.
  gst_ml_structure_get_source_region (pmeta->info, &region);

  mltype = GST_ML_FRAME_TYPE (mlframe);
  n_paxels = GST_ML_FRAME_DIM (mlframe, 0, 1);

  bboxes = GST_ML_FRAME_BLOCK_DATA (mlframe, 0);
  scores = GST_ML_FRAME_BLOCK_DATA (mlframe, 1);
  classes = GST_ML_FRAME_BLOCK_DATA (mlframe, 2);

  for (idx = 0; idx < n_paxels; idx++) {
    GstMLBoxEntry entry = { 0, };

    confidence = gst_ml_tensor_extract_value (mltype, scores, idx,
        submodule->qoffsets[1], submodule->qscales[1]);
    class_idx = gst_ml_tensor_extract_value (mltype, classes, idx,
        submodule->qoffsets[2], submodule->qscales[2]);

    // Discard results below the minimum score threshold.
    if (confidence < submodule->threshold)
      continue;

    entry.left = gst_ml_tensor_extract_value (mltype, bboxes, (idx * 4),
        submodule->qoffsets[0], submodule->qscales[0]);
    entry.top = gst_ml_tensor_extract_value (mltype, bboxes, (idx * 4 + 1),
        submodule->qoffsets[0], submodule->qscales[0]);
    entry.right  = gst_ml_tensor_extract_value (mltype, bboxes, (idx * 4 + 2),
        submodule->qoffsets[0], submodule->qscales[0]);
    entry.bottom  = gst_ml_tensor_extract_value (mltype, bboxes, (idx * 4 + 3),
        submodule->qoffsets[0], submodule->qscales[0]);

    GST_LOG ("Class: %u Confidence: %.2f Box[%.2f, %.2f, %.2f, %.2f]",
        class_idx, confidence, entry.top, entry.left, entry.bottom, entry.right);

    // Adjust bounding box dimensions with extracted source tensor region.
    gst_ml_box_transform_dimensions (&entry, &region);

    // Discard results with out of region coordinates.
    if ((entry.top > 1.0)    || (entry.left > 1.0)  ||
        (entry.bottom > 1.0) || (entry.right > 1.0) ||
        (entry.top < 0.0)    || (entry.left < 0.0)  ||
        (entry.bottom < 0.0) || (entry.right < 0.0))
      continue;

    label = g_hash_table_lookup (
        submodule->labels, GUINT_TO_POINTER (class_idx));

    entry.confidence = confidence * 100.0F;
    entry.name = g_quark_from_string (label ? label->name : "unknown");
    entry.color = label ? label->color : 0x000000F;

    // Non-Max Suppression (NMS) algorithm.
    nms = gst_ml_box_non_max_suppression (&entry, prediction->entries);

    // If the NMS result is -2 don't add the prediction to the list.
    if (nms == (-2))
      continue;

    GST_TRACE ("Label: %s Confidence: %.2f Box[%.2f, %.2f, %.2f, %.2f]",
        g_quark_to_string (entry.name), entry.confidence, entry.top, entry.left,
        entry.bottom, entry.right);

    // If the NMS result is above -1 remove the entry with the nms index.
    if (nms >= 0)
      prediction->entries = g_array_remove_index (prediction->entries, nms);

    prediction->entries = g_array_append_val (prediction->entries, entry);
  }

  g_array_sort (prediction->entries, (GCompareFunc) gst_ml_box_compare_entries);
}

static void
gst_ml_module_parse_dualblock_frame (GstMLSubModule * submodule,
    GArray * predictions, GstMLFrame * mlframe)
{
  GstProtectionMeta *pmeta = NULL;
  GstMLBoxPrediction *prediction = NULL;
  GstMLLabel *label = NULL;
  gpointer bboxes = NULL, scores = NULL;
  GstVideoRectangle region = { 0, };
  GstMLType mltype = GST_ML_TYPE_UNKNOWN;
  guint n_classes = 0, n_paxels = 0, idx = 0, num = 0, id = 0, class_idx = 0;
  gfloat confidence = 0;
  gint nms = -1;

  pmeta = gst_buffer_get_protection_meta_id (mlframe->buffer,
      gst_batch_channel_name (0));

  prediction = &(g_array_index (predictions, GstMLBoxPrediction, 0));
  prediction->info = pmeta->info;

  // Extract the source tensor region with actual data.
  gst_ml_structure_get_source_region (pmeta->info, &region);

  // The 2nd dimension represents the number of paxels.
  n_paxels = GST_ML_FRAME_DIM (mlframe, 0, 1);

  mltype = GST_ML_FRAME_TYPE (mlframe);
  if (GST_ML_FRAME_DIM (mlframe, 0, 2) == 4) {
    //Tensor dimensions looks like: <1, 8400, 4>, <1, 8400, 80>
    bboxes = GST_ML_FRAME_BLOCK_DATA (mlframe, 0);
    scores = GST_ML_FRAME_BLOCK_DATA (mlframe, 1);
    n_classes = GST_ML_FRAME_DIM (mlframe, 1, 2);
  } else {
    //Tensor dimensions looks like: <1, 8400, 80>, <1, 8400, 4>
    bboxes = GST_ML_FRAME_BLOCK_DATA (mlframe, 1);
    scores = GST_ML_FRAME_BLOCK_DATA (mlframe, 0);
    n_classes = GST_ML_FRAME_DIM (mlframe, 0, 2);
  }

  for (idx = 0; idx < n_paxels; idx++) {
    GstMLBoxEntry entry = { 0, };

    // Initial position ID of the class index.
    id = idx * n_classes;

    // Find the position of the class index with the highest score in current paxel.
    for (num = (id + 1); num < (id + n_classes); num++)
      id = (gst_ml_tensor_compare_values (mltype, scores, num, id) > 0) ? num : id;

    confidence = gst_ml_tensor_extract_value (mltype, scores, id,
        submodule->qoffsets[0], submodule->qscales[0]);

    // Get the class index from the position ID.
    class_idx = id % n_classes;

    // Discard results below the minimum score threshold.
    if (confidence < submodule->threshold)
      continue;

    if (class_idx != 0)
      continue;

    entry.left = gst_ml_tensor_extract_value (mltype, bboxes, (idx * 4),
        submodule->qoffsets[1], submodule->qscales[1]);
    entry.top = gst_ml_tensor_extract_value (mltype, bboxes, (idx * 4 + 1),
        submodule->qoffsets[1], submodule->qscales[1]);
    entry.right = gst_ml_tensor_extract_value (mltype, bboxes, (idx * 4 + 2),
        submodule->qoffsets[1], submodule->qscales[1]);
    entry.bottom = gst_ml_tensor_extract_value (mltype, bboxes, (idx * 4 + 3),
        submodule->qoffsets[1], submodule->qscales[1]);

    GST_LOG ("Class: %u Confidence: %.2f Box[%.2f, %.2f, %.2f, %.2f]",
        class_idx, confidence, entry.top, entry.left, entry.bottom, entry.right);

    // Adjust bounding box dimensions with extracted source tensor region.
    gst_ml_box_transform_dimensions (&entry, &region);

    // Discard results with out of region coordinates.
    if ((entry.top > 1.0)    || (entry.left > 1.0)  ||
        (entry.bottom > 1.0) || (entry.right > 1.0) ||
        (entry.top < 0.0)    || (entry.left < 0.0)  ||
        (entry.bottom < 0.0) || (entry.right < 0.0))
      continue;

    label = g_hash_table_lookup (submodule->labels, GUINT_TO_POINTER (class_idx));

    entry.confidence = confidence * 100.0F;
    entry.name = g_quark_from_string (label ? label->name : "unknown");
    entry.color = label ? label->color : 0x000000F;

    // Non-Max Suppression (NMS) algorithm.
    nms = gst_ml_box_non_max_suppression (&entry, prediction->entries);

    // If the NMS result is -2 don't add the prediction to the list.
    if (nms == (-2))
      continue;

    // If the NMS result is above -1 remove the entry with the nms index.
    if (nms >= 0)
      prediction->entries = g_array_remove_index (prediction->entries, nms);

    prediction->entries = g_array_append_val (prediction->entries, entry);
  }

  g_array_sort (prediction->entries, (GCompareFunc) gst_ml_box_compare_entries);
}

gboolean
gst_ml_module_process (gpointer instance, GstMLFrame * mlframe, gpointer output)
{
  GstMLSubModule *submodule = GST_ML_SUB_MODULE_CAST (instance);
  GArray *predictions = (GArray *) output;

  g_return_val_if_fail (submodule != NULL, FALSE);
  g_return_val_if_fail (mlframe != NULL, FALSE);
  g_return_val_if_fail (predictions != NULL, FALSE);

  if (GST_ML_INFO_N_TENSORS (&(submodule->mlinfo)) == 3) {
    gst_ml_module_parse_tripleblock_frame (submodule, predictions, mlframe);
  } else if (GST_ML_INFO_N_TENSORS (&(submodule->mlinfo)) == 2) {
    gst_ml_module_parse_dualblock_frame (submodule, predictions, mlframe);
  } else {
    GST_ERROR ("Ml frame with unsupported post-processing procedure!");
    return FALSE;
  }

  return TRUE;
}