<?php
/**
 * @file
 * Utility functions.
 */

/**
 * Log an object view if appropriate.
 *
 * @todo: Join up these db calls.
 */
function islandora_usage_stats_log_object_view(AbstractObject $object) {
  // Check if we should log.
  if (!islandora_usage_stats_should_not_log()) {
    $last_viewed = islandora_usage_stats_session_object_last_viewed_time($object->id);
    // Check if user has requested this item in the past X seconds.
    if (($last_viewed + variable_get('islandora_usage_stats_cooldown_seconds', 300)) < REQUEST_TIME) {
      module_load_include('inc', 'islandora_basic_collection', 'includes/utilities');
      module_load_include('inc', 'islandora_usage_stats', 'includes/db');
      $pid_id = islandora_usage_stats_get_pid_id($object->id);
      $object_access_id = islandora_usage_stats_object_view_to_db($pid_id);

      module_invoke_all('islandora_usage_stats_object_access_recorded', $object_access_id, $pid_id, $object->id);

      // Log content models.
      foreach ($object->models as $model) {
        if ($model != 'fedora-system:FedoraObject-3.0') {
          try {
            $model_id = islandora_usage_stats_get_pid_id($model);
            $model_access_id = islandora_usage_stats_content_model_view_to_db($object_access_id, $model_id);
            module_invoke_all('islandora_usage_stats_cmodel_access_recorded', $model_access_id, $model_id, $model, $object->id);
            $model_for_hook_name = preg_replace('/:/', '_', strtolower($model));
            module_invoke_all('islandora_usage_stats_' . $model_for_hook_name . '_access_recorded', $model_access_id, $model_id, $model, $object->id);
          }
          catch (Exception $e) {
            watchdog_exception(
              'islandora_usage_stats',
              $e,
              'Attempted load of %cmodel as cmodel of %obj',
              array('%cmodel' => $model, '%obj' => $object->id),
              WATCHDOG_ERROR);
          }
        }
      }

      // Log parents.
      
      $collections = islandora_basic_collection_get_parent_pids($object);
      if ($object && array_intersect(array('islandora:sp_pdf', 'islandora:intermediateSerialCModelStub'),$object->models)){
        $serialObject = $object->relationships->get(ISLANDORA_RELS_EXT_URI, ISLANDORA_SERIAL_OBJECT_PDF_PARENT_PREDICATE);
        if(is_array($serialObject) && isset($serialObject) && !empty($serialObject)){
          $parent_rels = reset($serialObject);
          $collections[] = $parent_rels['object']['value'];
        }else{
          $collections = islandora_basic_collection_get_parent_pids($object);
        }
      }else{
        $collections = islandora_basic_collection_get_parent_pids($object);
      }
      foreach ($collections as $collection) {
        try {
          $collection_id = islandora_usage_stats_get_pid_id($collection);
          $collection_access_id = islandora_usage_stats_collection_view_to_db($object_access_id, $collection_id);
          module_invoke_all('islandora_usage_stats_collection_access_recorded', $collection_access_id, $collection_id, $collection, $object_access_id, $object->id);
        }
        catch (Exception $e) {
          watchdog_exception(
            'islandora_usage_stats',
            $e,
            'Attempted load of %cmodel as parent collection of %obj',
            array('%cmodel' => $collection, '%obj' => $object->id),
            WATCHDOG_ERROR);
        }
      }
      islandora_usage_stats_session_object_last_viewed_time($object->id, REQUEST_TIME);
    }
  }
}

function islandora_usage_stats_find_all_level_parents($objectID,$level){
  if(!isset($level)){
    $level = 0;
  }
  $topParent = "cuhk:root";
  $rootParent = "islandora:root";
  module_load_include('inc', 'islandora_basic_collection', 'includes/utilities');
  
  $object = islandora_object_load($objectID);
  $parents = NULL;
  if ($object && array_intersect(array('islandora:sp_pdf', 'islandora:intermediateSerialCModelStub'),$object->models)){
    $serialObject = $object->relationships->get(ISLANDORA_RELS_EXT_URI, ISLANDORA_SERIAL_OBJECT_PDF_PARENT_PREDICATE);
    if(is_array($serialObject) && isset($serialObject) && !empty($serialObject)){
      $parent_rels = reset($serialObject);
      $parents[] = $parent_rels['object']['value'];
    }else{
      $parents = islandora_basic_collection_get_parent_pids($object);
    }
  }else if($object){
    $parents = islandora_basic_collection_get_parent_pids($object);
  }
  if($parents){
    foreach($parents as $parent){
      if($parent !== $topParent && $parent !== $rootParent){
          $parentArray[$level]['parent_id'] = $objectID;
          $parentArray[$level]['parent_name'] = $object->label;
          $parentArray = array_merge($parentArray,islandora_usage_stats_find_all_level_parents($parent,$level+1));
      }else if($level === 0){
        $parentObject = islandora_object_load($parent);
        $parentArray[$level]['parent_id'] = $parentObject->id;
        $parentArray[$level]['parent_name'] = $parentObject->label;
      }else{
        $parentArray[$level]['parent_id'] = $objectID;
        $parentArray[$level]['parent_name'] = $object->label;
      }
    }
  return $parentArray;
  }else{
    return NULL;
  }
}

/**
 * Logs the download of a datastream.
 */
function islandora_usage_stats_log_datastream_download($pid, $dsid) {
  // Check if user's ip is excluded.
  if (!islandora_usage_stats_should_not_log()) {
    module_load_include('inc', 'islandora_usage_stats', 'includes/utilities');
    $last_viewed = islandora_usage_stats_session_datastream_last_viewed_time($pid, $dsid);
    // User has never viewed item before.
    // Check if user has requested this item in the past X seconds.
    if (($last_viewed + variable_get('islandora_usage_stats_cooldown_seconds', 300)) < REQUEST_TIME) {
      module_load_include('inc', 'islandora_usage_stats', 'includes/db');
      islandora_usage_stats_log_object_view(islandora_object_load($pid));
      $ds_dsid = islandora_usage_stats_get_ds_dsid($pid, $dsid);
      $datastream_download_id = islandora_usage_stats_datastream_download_to_db($ds_dsid);
      module_invoke_all('islandora_usage_stats_datastream_download_recorded', $datastream_download_id, $ds_dsid, $pid, $dsid);
      islandora_usage_stats_session_datastream_last_viewed_time($pid, $dsid, REQUEST_TIME);
    }
  }
}

/**
 * Track the top used search terms.
 *
 * @param string $term
 *   The search text to track.
 */
function islandora_usage_stats_track_search_term($term) {
  module_load_include('inc', 'islandora_usage_stats', 'includes/db');
  $term = strtolower($term);
  $result = islandora_usage_stats_get_search_term($term);
  if ($result) {
    // Term is found, increment by one.
    islandora_usage_stats_increment_search_term($result['id']);
  }
  else {
    islandora_usage_stats_set_search_term($term);
  }
}

/**
 * Check if log entry should be created.
 *
 * Performs check to see if user's IP is excluded from tracking. Also checks
 * if user is a bot if enabled.
 *
 * @return bool
 *   TRUE if excluded from tracking, FALSE otherwise.
 */
function islandora_usage_stats_should_not_log() {
  $ignored_ips = explode(' ', variable_get('islandora_usage_stats_ip_list', 'localhost 127.0.0.1'));
  $ip = ip_address();

  // Check if user's IP is excluded.
  foreach ($ignored_ips as $ignored_ip) {

    // Is user's IP directly on the IP exclusion list?
    if ($ip == $ignored_ip) {
      return TRUE;
    }

    // Is entry in IP exclusion list actually a CIDR range?
    elseif (strpos($ignored_ip, "/")) {
      list($net, $mask) = explode("/", $ignored_ip);
      $net_long = ip2long($net);
      $ip_mask = ~((1 << (32 - $mask)) - 1);
      $ip_long = ip2long($ip);
      $ip_ip_net = $ip_long & $ip_mask;

      // Does user's IP fall into this CIDR range?
      if ($ip_ip_net == $net_long) {
        return TRUE;
      }
    }
  }

  // Bots check here.
  if (variable_get('islandora_usage_stats_exclude_bots', 1) && isset($_SERVER['HTTP_USER_AGENT'])) {
    $bot_regex = variable_get('islandora_usage_stats_exclude_bots_regex', '/bot|rambler|spider|crawl|slurp|curl|^$/i');
    if (1 === preg_match($bot_regex, $_SERVER['HTTP_USER_AGENT'])) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * Get or set the session object last viewed time.
 *
 * @param string $pid
 *   PID to check or set.
 * @param mixed $time
 *   Timestamp to set the last view on the PID to or NULL.
 *
 * @return mixed
 *   NULL or a string of an integer since the epoch.
 */
function islandora_usage_stats_session_object_last_viewed_time($pid, $time = NULL) {
  if (!$time) {
    return @$_SESSION['islandora_usage_stats']['objects_viewed'][$pid]['requested_time'];
  }
  $_SESSION['islandora_usage_stats']['objects_viewed'][$pid]['requested_time'] = $time;
}

/**
 * Get or set the session datastream last viewed time.
 *
 * @param string $pid
 *   PID to check or set.
 * @param string $dsid
 *   DSID to check or set.
 * @param mixed $time
 *   Timestamp to set the last view on the PID to or NULL.
 *
 * @return mixed
 *   NULL or a string of an integer since the epoch.
 */
function islandora_usage_stats_session_datastream_last_viewed_time($pid, $dsid, $time = NULL) {
  if (!$time) {
    return @$_SESSION['islandora_usage_stats']['datastreams_viewed'][$pid][$dsid]['requested_time'];
  }
  $_SESSION['islandora_usage_stats']['datastreams_viewed'][$pid][$dsid]['requested_time'] = $time;
}
