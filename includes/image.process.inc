<?php
/**
 * @file
 * This file contains all the functions for image manipulation used in the
 * basic image solution pack.
 */

/**
 * creates the derivatives for this content model type
 * @param object $object
 *   tuque FedoraObject
 * @return string
 *
 * @TODO: is the exif derivative missing here?
 */
function islandora_basic_image_create_all_derivatives($object) {
  module_load_include('inc', 'islandora', 'includes/mime.detect');
  $mime_class = new MimeDetect();
  if (!isset($object['OBJ'])) {
    drupal_set_message(t('Could not create image derivatives for %s.  No image file was uploaded.', array('%s' => $object->id)), 'error');
    return "";
  }

  /// TODO: This solution pack should really check for proper type uploaded.. the PHP file type detection code is very poor.

  $ext = $mime_class->getExtension($object['OBJ']->mimeType);
  $file_name = str_replace(':', '-', $object->id);

  // When we get a tiff, we need to save the original to an ARCHIVE datastream and replace the OBJ with a down-sampled JPG

  if ($ext == 'tiff' || $ext == 'tif') {

    $archive_file = file_save_data($object['OBJ']->content, 'temporary://' . $file_name . 'ARCHIVE.tiff');
    $master_file  = file_save_data('', 'temporary://' . $file_name . 'OBJ.jpg');
    $old_label = $object['OBJ']->label;

    exec(escapeshellcmd("convert -resize 1024x1024> " . drupal_realpath($archive_file->uri)) . " " . drupal_realpath($master_file->uri));

    $ext = 'jpg';

    islandora_basic_image_add_datastream($object, 'ARCHIVE', $archive_file, 'Original Archival TIFF from ' . $old_label, 'image/tiff');

    /// TODO: use $object['OBJ']->modifyDatastream instead?
    /// need to reset label, mimetype, versionable, content

    // At this point we've still got a tiff as the main object (dsid OBJ) - let's fix that.

    if ($object->purgeDatastream('OBJ')) {      

      $ds = $object->constructDatastream('OBJ', 'M');
      $ds->label = 'JPEG Derived from Archival TIFF Image';
      $ds->mimeType = 'image/jpeg';
      $ds->versionable = FALSE;
      $ds->setContentFromFile(drupal_realpath($master_file->uri));
      $object->ingestDatastream($ds);

      // 

      $ds = $object->constructDatastream('POLICY', 'M');
      $ds->label = 'Deny TIFF to Anonymous';
      $ds->mimeType = 'application/rdf+xml';
      $ds->versionable = TRUE;
      $ds->setContentFromFile(drupal_get_path('module', 'islandora_basic_image') . '/includes/Deny-Archival-TIFF-to-Anonymous.rdf');
      $object->ingestDatastream($ds);

    } else {

      drupal_set_message(t("Could not properly create and save a JPEG image from the submitted TIFF."));

      // TODO: My my, what to do here for cleanup?
    }

  // Not a TIFF:

  } else {
    $master_file = file_save_data($object['OBJ']->content, 'temporary://' . $file_name . 'OBJ.' . $ext);
  }

  $tn_file = file_copy($master_file, 'temporary://' . $file_name . 'TN.' . $ext);

  if (islandora_basic_image_create_derivative($tn_file, 200, 200)) {
    islandora_basic_image_add_datastream($object, 'TN', $tn_file, 'Thumbnail Image');
  }

  $medium_file = file_copy($master_file, 'temporary://' . $file_name . 'MEDIUM.' . $ext);

  if (islandora_basic_image_create_derivative($medium_file, 500, 700)) {
    islandora_basic_image_add_datastream($object, 'MEDIUM_SIZE', $medium_file, 'Medium Size Image');
  }

  file_delete($master_file);
}


/**
 *
 * @param object $file
 *   stdclass
 *
 * @TODO: could this be a generic function? eg: islandora_create_derivative($file, $width, $height). ?
 */
function islandora_basic_image_create_derivative($file, $width, $height) {
  $real_path = drupal_realpath($file->uri);
  $image = image_load($real_path);
  if (!empty($image)) {
    $scale = image_scale($image, $width, $height, TRUE);
    if ($scale) {
      return image_save($image);
    }
  }
  return FALSE;
}

/**
 * adds a datastream and deletes the tmp file from local file system
 * @param object $object
 * @param string $dsid
 * @param object $file
 * @param string $label
 * @param optional string $mime_type
 *
 * @TODO: could this be a generic function? eg: islandora_add_datastream($object, $dsid, $file). ?
 */

function islandora_basic_image_add_datastream($object, $dsid, $file, $label, $mime_type = null) {
  try {
    $ds = $object->constructDatastream($dsid, 'M');
    $ds->label = $label;
    if (is_null($mime_type)) {
      $ds->mimeType = $object['OBJ']->mimeType;
    } else {
      $ds->mimeType = $mime_type;
    }      
    $ds->setContentFromFile(drupal_realpath($file->uri));
    $object->ingestDatastream($ds);
    file_delete($file);
  } catch (exception $e) {
    drupal_set_message(t('@message', array('@message' => check_plain($e->getMessage()))), 'error');
  }
}

