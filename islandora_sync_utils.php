<?php

global $our_content_models;
$our_content_models = array(
	"epistemetec:mag_img",
	"epistemetec:mag_big_img",
	"epistemetec:mag_book",
	"epistemetec:mag_audio",
	"epistemetec:mag_video",
	"epistemetec:mag_doc",
);

/*
 * Here we can handle the number of the book's pages
function __getBookPages($pid) {

	return "pagine di $pid";
}
*/

/**
 * Create or update a node from a pid/cm
 *
 * @param string $pid - fedora repository object id
 * @param string $cm - content model fo the object
 */
function __manage_node($pid, $cm) {
	$default_dsID = variable_get('islandora_sync_default_dsid', 'MAG');
	$drupal_dsID = variable_get('islandora_sync_drupal_dsid', 'RELS-DRUPAL');

	module_load_include('inc', 'fedora_repository', 'ObjectHelper');
	$objectHelper = new ObjectHelper();

	$ds_info = $objectHelper->getStream($pid, $default_dsID);
	if (!isset($ds_info) || empty($ds_info)) {
		watchdog("islandora_sync", "Can't get datastream info from pid: !pid", array('pid' => $pid), WATCHDOG_ERROR);
		drupal_set_message("Can't get datastream info from pid {$pid}", $type = 'error');
		return -1;
	}
	//file_save_data($ds_info, 'manage_node_debug.txt', FILE_EXISTS_RENAME); //debug

	$xml_array_values = __mag_xml_to_array($ds_info); //TODO move it


	//add extra values

	$xml_array_values['pid'] = $pid;

	$xml_array_values['collection_pid'] = __getCollectionPid($pid);

	//$xml_array_values['book_nof_pages'] = __getBookPages($pid);

	//check relations between drupal nodes and datastream
	$actions = __check_drupal_rel($pid);

	if (isset($actions["create-node"])) {
		$node_type = __getNodeTypeAssoc($cm);

		if (!$node_type) {
			watchdog("islandora_sync", "Errors retrieving node type from !pid", array('!pid' => $pid), WATCHDOG_ERROR);
			drupal_set_message("Errors retrieving node type from {$pid}", $type = 'error');
			return -1;
		}

		$nid = createNode($xml_array_values, $node_type);

		if (!isset($nid)) {
			watchdog("islandora_sync", "Errors creating a Drupal Node for the Fedora Object !pid", array('!pid' => $pid), WATCHDOG_ERROR);
			drupal_set_message("Errors creating a Drupal Node for the Fedora Object {$pid}", $type = 'error');
			return -1;
		}
		else {
			drupal_set_message("Node $nid was created for object $pid", 'notice');
		}
	}
	elseif(isset($actions["update-node"])) {
		$nid = $actions["update-node"];
		$updated = updateNode($xml_array_values, $nid);
		if (!$updated) {
			watchdog("islandora_sync", "Errors updating the Drupal Node !nid for the Fedora Object !pid",	array('!nid' => $nid, '!pid' => $pid), WATCHDOG_ERROR);
			drupal_set_message("Errors creating a Drupal Node for the Fedora Object {$pid}", $type = 'error');
			return -1;
		}
	}

	if (isset($actions["create-datastream"])) {
		watchdog("islandora_sync", "Creating drupal rel datastream between object @pid and node @nid ...", array('@nid' => $nid, '@pid' => $pid),  WATCHDOG_NOTICE);
		createBaseDrupalRelDatastream($pid);
	}

	if (isset($actions["create-rel"])) {
		watchdog("islandora_sync", "Creating drupal rel stanza between object @pid and node @nid ...", array('@nid' => $nid, '@pid' => $pid),  WATCHDOG_NOTICE);
		createRelOnDrupalRelDatastream($pid, $nid);
	}
	elseif (isset($actions["update-rel"])) {
		watchdog("islandora_sync", "Updating drupal rel stanza between object @pid and node @nid ...", array('@nid' => $nid, '@pid' => $pid),  WATCHDOG_NOTICE);
		updateRelOnDrupalRelDatastream($pid, $nid);
	}

	if (isset($actions["message"])) {
		$watchdog_level = "WATCHDOG_" . $actions["message-level"];
		watchdog("islandora_sync", $actions["message"], array(), $watchdog_level);
	}

}


/**
 * Check if ther is a relation on the object datastream RELS-DRUPAL.
 * If so, check if the node exists and return what type of action must be done.
 * returns an array that can contain one or more of the following actions:
 *
 * 'create-datastream' => unset/true
 * 'create-rel' => unset/true
 * 'create-node' => unset/true
 * 'update-rel' => unset/true (use the $nid from the new node or from the one updated)
 * 'update-node' => unset/$nid
 * 'message' => unset/text message
 * 'message-level' => unset/level:error,warning,notice
 *
 * @param string $pid - fedora object id
 * @return array $actions - contains actions
 */
function __check_drupal_rel($pid) {
	$fedora_nid = __getNidFromFedora($pid);
	$drupal_nid = __getNidFromDrupal($pid);

	switch (count($drupal_nid)) {
		case 0:
			//non ci sono nodi drupal correlati a questo pid...
			if ($fedora_nid == -1) {
				//... e non c'è neppure il datastream "rel-drupal": crea il nodo, il datastream e la stanza al suo interno con il nid
				$actions = array(
					'create-datastream' => true,
					'create-rel' => true,
					'create-node' => true
				);
			}
			elseif ($fedora_nid == 0) {
				//... c'è rel-drupal ma non c'è una stanza per questo pid: creala inserendo il nid del nuovo nodo che verrà creato
				$actions = array(
					'create-rel' => true,
					'create-node' => true
				);
			}
			else {
				//... c'è "rel-drupal" e la stanza: il valore del nid all'interno della stanza va modificato con quello del nuovo nodo che verrà creato
				$actions = array(
					'create-node' => true,
					'update-rel' => true
				);
			}
			break;
		case 1:
			//... c'è un nodo drupal correlato a questo pid...
			if ($fedora_nid == -1) {
				//... ma non c'è il datastream "rel-drupal": crea il datastream e la stanza al suo interno con il nid di questo nodo
				$actions = array(
					'create-datastream' => true,
					'create-rel' => true,
					'update-node' => $drupal_nid[0],
					'message' => "Using node {$drupal_nid[0]} as related node to {$pid}",
					'message-level' => "NOTICE"
				);
			}
			elseif ($fedora_nid == 0) {
				//... c'è rel-drupal ma non c'è una stanza per questo pid: creala inserendo il nid del nodo presente
				//TODO in realtà qui ci finisce anche se c'è la stanza "master" ma è sbagliata (es. url vecchia)
				$actions = array(
					'create-rel' => true,
					'update-node' => $drupal_nid[0],
					'message' => "Using node {$drupal_nid[0]} as related node to {$pid}",
					'message-level' => "NOTICE"
				);
			}
			else {
				//... c'è il datastream e la stanza: il nid drupal ha la meglio su quello presente nel datastream se diversi
				if ($drupal_nid[0] == $fedora_nid) {
					$actions = array(
						'update-node' => $drupal_nid[0]
					);
				}
				else {
					$actions = array(
						'update-rel' => true,
						'update-node' => $fedora_nid,
						'message' => "Updating datastream with node {$drupal_nid[0]} as related to {$pid}",
						'message-level' => "NOTICE"
					);
				}
			}
			break;
		default:
			//c'è più di un nodo drupal correlato con questo pid
			$drupal_nid_string = implode(", ", $drupal_nid);

			if (in_array($fedora_nid, $drupal_nid)) {
				//uno di questi nodi ha una correlazione con la stanza nel rel-drupal; lascia intatto il datastream e aggiorna il nodo
				//gli altri nodi che non hanno una corrispondenza vanno controllati manualmente
				$actions = array(
					'update-node' => $fedora_nid,
					'message' => "There are many nodes ({$drupal_nid_string}) associated with the pid ({$pid}) and one of them ({$fedora_nid}) is on the datastream fedora, so it's been updated. You need to check the other Drupal nodes manually.",
					'message-level' => "WARNING"
				);
			}
			else {
				//più nodi si riferiscono a questo pid ma non ci sono informazioni sui datastream dell'oggetto fedora;
				//poiché non sappiamo qual'è il nodo corretto non possiamo fare nulla.
				$actions = array(
					'message' => "There are many nodes ({$drupal_nid_string}) associated with the pid ({$pid}) but no information (correct) on the Fedora datastream. I can't do anything: Drupal nodes should be checked manually.",
					'message-level' => "ERROR"
				);
			}
	}

	return $actions;
}

/**
 * Retrieves the nid from the object datastream
 *
 * @param string $pid - fedora object id
 * @return -1: drupal-rel datastream not found; 0: relationship not found; $nid: node related.
 */
function __getNidFromFedora($pid) {
	module_load_include('inc', 'fedora_repository', 'ObjectHelper');
	$objectHelper = new ObjectHelper();

	$drupal_dsID = variable_get('islandora_sync_drupal_dsid', 'RELS-DRUPAL');
	$drupal_info = $objectHelper->getStream($pid, $drupal_dsID);

	if (!isset($drupal_info)) {
		return -1; //there is not drupal-rel: you have to create it
	}
	else {
		global $base_url;

	    $dom = new DomDocument();
	    $dom->preserveWhiteSpace = false;
	    $dom->loadXML($drupal_info);

	    $xpath = new DomXPath($dom);
		$xpath->registerNamespace("php", "http://php.net/xpath");
		$xpath->registerPHPFunctions();

		//scroll all the possible elements searching for this base_url
		$b_urls = $xpath->query('//base_url');
		foreach ($b_urls as $b_url) {
			if ($b_url->nodeValue == $base_url) { //the url has been taken, then take the nid
				$nid = $b_url->parentNode->lastChild->nodeValue;
				return $nid;
			}
		}

		return 0; //stanza not found
	}
}

/**
 * Retrieves the nid from the Drupal CCK field_fedora_pid
 *
 * @param string $pid - fedora object id
 * @return nid, nids or empty array if none.
 */
function __getNidFromDrupal($pid) {
	$drupal_nid = array();

	//check if a drupal node(s) contains this pid on field_fedora_pid's cck
	$sql = "SELECT node.nid AS nid
		FROM node node
 			LEFT JOIN content_field_fedora_pid node_data_field_fedora_pid ON node.vid = node_data_field_fedora_pid.vid
 		WHERE (node.type in ('fo_audio', 'fo_book', 'fo_doc', 'fo_img', 'fo_big_img', 'fo_video'))
 			AND (node_data_field_fedora_pid.field_fedora_pid_value = '" . $pid . "')";

	$result = db_query($sql);
	while ($row = db_fetch_object($result)) {
	  $drupal_nid[] = $row->nid;
	}

	return $drupal_nid;
}


/**
 * Convert form_values keys to cck fields names.
 *
 * @param object $node - the node that will be created
 * @param array $form_values - values ingested
 * @param string $type - the node type in a machine readable form
 * @param boolean $isEditing
 */
function __hashCCK(&$node, $form_values, $type, $isEditing = FALSE) {
	$node_type = __getNodeTypeName($type);

	$all_ccks = variable_get('islandora_sync_ccks', array());
	if (isset($all_ccks[$node_type])) {
		$type_ccks = $all_ccks[$node_type];

		foreach ($type_ccks as $cck => $value) {
			if (isset($form_values[$value])) {
				//here we can distinguish the cck type
				$field = content_fields($cck);
				if ($field['type'] == "content_taxonomy") {
					if ($field['widget']['type'] == "content_taxonomy_autocomplete") {
						//TODO: extend to map multiple taxonomy values...

						//e.g. content taxonomy field needs the term id to retrieve and insert automatically the term name in this widget type
						$single_value = is_array($form_values[$value]) ? $form_values[$value][count($form_values[$value])-1] : $form_values[$value];
						if ($value == "collection_pid") {
							$val = __getCollectionTidByPid( (string) $single_value );
						}
						else {
							$term = taxonomy_get_term_by_name($single_value);
							$val = $term[0]->tid;
						}
					}
				}
				else {
					//TODO: extend to map multiple values...

					$single_value = is_array($form_values[$value]) ? $form_values[$value][count($form_values[$value])-1] : $form_values[$value];
					$val = $single_value;
				}

				//eval ("\$node->" . $cck . "[0]['value']=\"$val\";");
				$node->{$cck}[0]['value'] = $val;

			}
		}
		$node->field_fedora_pid[0]['value'] = $form_values['pid'];
	}
	else {
		$separator = variable_get('islandora_sync_metadata_namespace_separator', ':');
		$prefix = variable_get('islandora_sync_fedora_cck_field_prefix', 'fedora_');

		foreach ($form_values as $key => $value) {
			$cck = 'field_' . $prefix . str_replace($separator, '_', $key);

			if ($value) {
				$node->{$cck}[0]['value'] = $value;
			}
		}
	}
}


/**
 * Creates a new Drupal Node from Fedora Object's values
 *
 * @param array $form_values - values to fill ccks
 * @param string $type - content type for this node
 * @return $nid on success; false otherwise
 */
function createNode($form_values, $type) {
	if (!isset($type) OR !isset($form_values['pid'])) {
		return false;
	}

	global $user;
	$old_user = $user;
	$user = user_load(1);
	$node_url = '/fedora/repository/' . $form_values['pid'];

	if (variable_get('islandora_sync_translation_enabled', 0) == 0 || !isset($form_values["metadigit_lang"])) {
		$lang = ""; //language neutral
	}
	else {
		$lang = $form_values["metadigit_lang"];
	}

	// add node properties
	$node = new stdClass();
	$node->type = $type;
	$node->title = $form_values['dc:title']; //TODO:handle on module configuration page
	$node->uid = $user->uid;
	$node->language = $lang;
	$node->created = time();
	$node->changed = $node->created;
	$node->status = 1;
	$node->comment = 0;
	$node->promote = 0;
	$node->moderate = 0;
	$node->sticky = 0;

	//We add CCK field data to the node. To be sure to use only valid ccks for this type of node we extract all cck fields names
	// sing content_field api call. Then we execute the assignement where the $key from $ccks hash table is one of the cck fields.
	__hashCCK($node, $form_values, $type);

	//TODO add islandora_sync_createnode_alter hook passando $node e $ccks
	//TODO spostare in islandora_mag invocando l'hook
	$node->body = "<a href=\"" . $node_url . "\">" . $form_values['dc:title'] . "</a>";
	
	__createNodeImage(&$node, $form_values['pid']);

	node_save($node);
	$user = $old_user; //restore user

	return trim($node->nid);
}

/*
 * http://www.trellon.com/content/blog/data-migration-importing-images
 */
function __createNodeImage(&$node, $pid) {
	global $base_url;
	
	$image_path = $base_url . '/fedora/repository/' . $pid . "/PRE";

	if ($image_path) {
	  $binary_image = drupal_http_request($image_path);
	  
	  if ($binary_image->code == 200 OR $binary_image->code == 302) {
	    $filename = "islandora_sync-" . $pid . ".PRE.jpg";

	    $dst = file_create_path(file_directory_temp()) .'/'. $filename;
	    $temp_file = file_save_data($binary_image->data, $dst);
	    
	    if ($temp_file) {
	      $path = file_create_path() .'/'. $filename;
	      $node->field_dl_image[0] = field_file_save_file($temp_file, array(), $path, FILE_EXISTS_RENAME);
	    }
	  }
	  elseif ($binary_image->code == 401) {
	  	/* TODO temp basic authentication for www2 */
	  	  $url = "http://epistemetec:buffalo20@" . substr($base_url, 7);
	  	  $image_path = $url . '/fedora/repository/' . $pid . "/PRE";
	  	  
  		  $binary_image = drupal_http_request($image_path);
	  	
	  	  if ($binary_image->code == 200 OR $binary_image->code == 302) {
		    $filename = "islandora_sync-" . $pid . ".PRE.jpg";
	
		    $dst = file_create_path(file_directory_temp()) .'/'. $filename;
		    $temp_file = file_save_data($binary_image->data, $dst);
		    
		    if ($temp_file) {
		      $path = file_create_path() .'/'. $filename;
		      $node->field_dl_image[0] = field_file_save_file($temp_file, array(), $path, FILE_EXISTS_RENAME);
		    }
		  }
		  else {
		  	watchdog('islandora_sync_utils', "Error @code '@error' loading image @image for pid: @pid - 2nd retry", array( '@pid' => $pid, '@code' => $binary_image->code, '@error' => $binary_image->error, '@image' => $image_path ), WATCHDOG_ERROR);
		  }
	  }
	  else {
	  	watchdog('islandora_sync_utils', "Error @code '@error' loading image @image for pid: @pid -1st try", array( '@pid' => $pid, '@code' => $binary_image->code, '@error' => $binary_image->error, '@image' => $image_path ), WATCHDOG_ERROR);
	  }
	}
	
}

/**
 * Update an existing Drupal Node from Fedora Object's values
 *
 * @param array $form_values - values to update ccks
 * @param int $nid - node to be updated
 * @return true on success; false otherwise
 */
function updateNode($form_values, $nid) {
	if (!isset($nid)) {
		return false;
	}
	
	global $user;
	$old_user = $user;
	$user = user_load(1);
	$node = node_load($nid);
	
	__hashCCK($node, $form_values, $node->type);
	
	if (!isset($node->field_dl_image[0])) {
		__createNodeImage(&$node, $form_values['pid']);
	}

	node_save($node);
	
	$user = $old_user; //restore user

	return true;
}

/**
 * Remove the dupal node if exist
 *
 * @param int $nid - Drupal node ID
 */
function deleteNode($nid) {
	$node_exist = node_load($nid);

	if ($node_exist){
		node_delete($nid);
		drupal_set_message(t('The Drupal node: @nid, was deleted successfully.', array('@nid' => $nid)));
	}
}

/**
 * Creates the Datastream into Fedora Object that will be used to hanlde
 * relationship with Drupal Nodes and to know what Frontend is using it.
 *
 * @param string $pid - Fedora Object ID
 * @return true on success; false otherwise
 */
function createBaseDrupalRelDatastream($pid) {
	if (empty($pid)) {
		return false;
	}

	$drupal_dsID = variable_get('islandora_sync_drupal_dsid', 'RELS-DRUPAL');

	$dom = new DomDocument("1.0", "UTF-8");
	$dom->formatOutput = TRUE;

	module_load_include('inc', 'fedora_repository', 'api/fedora_item');
	$fedora_object = new Fedora_Item($pid);

	$drupal_rel = $dom->createElement("drupal_rel");
	$dom->appendChild($drupal_rel);

	$master_rel = $dom->createElement("master");
	$slaves_rel = $dom->createElement("slaves");

	$drupal_rel->appendChild($master_rel);
	$drupal_rel->appendChild($slaves_rel);

	global $user;
	$old_user = $user;
	$user = user_load(1);

	if ($fedora_object->add_datastream_from_string($dom->saveXML(), $drupal_dsID, 'Drupal Rel Metadata', 'text/xml', 'X') !== NULL) {
		sleep(1);
		$user = $old_user;
		return true;
	}
	else {
		watchdog('islandora_sync_utils', "Failed to creare RELS-DRUPAL  pid: @pid", array( '@pid' => $pid ),WATCHDOG_ERROR);
		$user = $old_user;
		return false;
	}

}

/**
 * Insert information about the Node related to this pid and this Frontend into the Datastream
 *
 * @param string $pid - Fedora Object ID
 * @param int $nid - Drupal Node ID
 * @return true on success; false otherwise
 */
function createRelOnDrupalRelDatastream($pid, $nid, &$dom = false) {
	if (empty($pid) or empty($nid)) {
		return false;
	}

	$is_master = variable_get("islandora_sync_is_master", 0);
	$drupal_dsID = variable_get('islandora_sync_drupal_dsid', 'RELS-DRUPAL');
	global $base_url;
	$node_url = $base_url . "/node/" . $nid;

	if ($dom == false) {
		$dom = new DomDocument("1.0", "UTF-8");
		$dom->formatOutput = TRUE;

		module_load_include('inc', 'fedora_repository', 'ObjectHelper');
		$objectHelper = new ObjectHelper();

		$drupal_info = $objectHelper->getStream($pid, $drupal_dsID);

		if (empty($drupal_info)) {
			 watchdog('islandora_sync_utils', "Error loading DRUPAL-REL  pid: @pid - nid: @nid", array( '@pid' => $pid, '@nid' => $nid ),WATCHDOG_ERROR);
			 return;
		}

		$dom->loadXML($drupal_info);
	}

	$xpath = new DomXPath($dom);
	$xpath->registerNamespace("php", "http://php.net/xpath");
	$xpath->registerPHPFunctions();

	if ($is_master) {
		$drupal_rel = $xpath->query('//drupal_rel/master');
	}
	else {
		$drupal_rel = $xpath->query('//drupal_rel/slaves');
	}

	if ($drupal_rel == FALSE OR $drupal_rel->length == 0) { //0: not found; NULL: query error
		watchdog('islandora_sync_utils', "Check DRUPAL-REL for pid: @pid - It seems to be wrong or xpath is kidding me...", array( '@pid' => $pid ),WATCHDOG_ERROR);
		return false;
	}
	$drupal_rel = $drupal_rel->item(0);

	if ($is_master) {
		$info_rel = $drupal_rel;
	}
	else {
		$info_rel = $dom->createElement("slave");
		$drupal_rel->appendChild($info_rel);
	}

	$ds_base_url = $dom->createElement("base_url", $base_url);
	$ds_node_url = $dom->createElement("node_uri", $node_url);
	$ds_nid = $dom->createElement("nid", $nid);

	$info_rel->appendChild($ds_base_url);
	$info_rel->appendChild($ds_node_url);
	$info_rel->appendChild($ds_nid);

	module_load_include('inc', 'fedora_repository', 'api/fedora_item');
	$fedora_object = new Fedora_Item($pid);

	global $user;
	$old_user = $user;
	$user = user_load(1);

	if ($fedora_object->modify_datastream_by_value($dom->saveXML(), $drupal_dsID, "Fedora Object to Druapl relationship", 'text/xml') !== NULL) {
		sleep(1);
		$user = $old_user;
		return true;
	}
	else {
		watchdog('islandora_sync_utils', "Failed to creare stanza on DRUPAL-REL  pid: @pid", array( '@pid' => $pid ),WATCHDOG_ERROR);
		$user = $old_user;
		return false;
	}

}

/**
 * Updates information about the Node related to this pid and this Frontend into the Datastream
 *
 * @param string $pid - Fedora Object ID
 * @param int $nid - Drupal Node ID
 */
function updateRelOnDrupalRelDatastream($pid, $nid) {
	if (empty($pid) or empty($nid)) {
		return false;
	}

	$drupal_dsID = variable_get('islandora_sync_drupal_dsid', 'RELS-DRUPAL');
	global $base_url;
	$node_url = $base_url . "/node/" . $nid;

	$dom = new DomDocument("1.0", "UTF-8");
	$dom->formatOutput = TRUE;

	module_load_include('inc', 'fedora_repository', 'ObjectHelper');
	$objectHelper = new ObjectHelper();

	$drupal_info = $objectHelper->getStream($pid, $drupal_dsID);
	$dom->loadXML($drupal_info);

	$xpath = new DomXPath($dom);
	$xpath->registerNamespace("php", "http://php.net/xpath");
	$xpath->registerPHPFunctions();

	$base_urls = $xpath->query('//base_url');

	if ($base_urls == FALSE OR $base_urls->length == 0) { //0: not found; NULL: query error
		return false;
	}

	foreach ($base_urls as $b_url) {
		if ($b_url->nodeValue == $base_url) {	//the url has been taken, then take the nid
			$old_nid = $b_url->parentNode->lastChild->nodeValue;

			$deleted = deleteRelOnDrupalRelDatastream($pid, $old_nid, $dom);
			if ($deleted) {
				$created = createRelOnDrupalRelDatastream($pid, $nid, $dom);

				if ($created) {
					watchdog('islandora_sync_utils', "Drupal Rel updated successfully for pid: @pid at nid: @nid", array('pid' => $pid, 'nid' => $nid),WATCHDOG_NOTICE);
					return true;
				}
				else {
					watchdog('islandora_sync_utils', "Error re-creating Drupal Rel on update process for pid: @pid at nid: @nid", array('pid' => $pid, 'nid' => $nid),WATCHDOG_ERROR);
					return false;
				}
			}
			else {
				watchdog('islandora_sync_utils', "Error deleting Drupal Rel on update process for pid: @pid at nid: @nid", array('pid' => $pid, 'nid' => $nid),WATCHDOG_ERROR);
				return false;
			}
		}
	}

	watchdog('islandora_sync_utils', "Error base_url not found on update process for pid: @pid at nid: @nid", array('pid' => $pid, 'nid' => $nid),WATCHDOG_ERROR);
	return false;
}

/**
 * Remove the relationship in the Drupal-Rel datastream.
 *
 * @param string $pid
 * @param int $nid
 */
function deleteRelOnDrupalRelDatastream($pid, $nid, &$dom = false) {
	if (empty($pid) or empty($nid)) {
		return false;
	}

	$drupal_dsID = variable_get('islandora_sync_drupal_dsid', 'RELS-DRUPAL');
	global $base_url;

	if ($dom == false) {
		$dom = new DomDocument("1.0", "UTF-8");
		$dom->formatOutput = TRUE;

		module_load_include('inc', 'fedora_repository', 'ObjectHelper');
		$objectHelper = new ObjectHelper();

		$drupal_info = $objectHelper->getStream($pid, $drupal_dsID);
		if ( empty( $drupal_info ) ) {
			return false;
		}

		$dom->loadXML($drupal_info);
	}

	$base_urls = $dom->getElementsByTagName('base_url');

	foreach ($base_urls as $b_url) {
		if ($b_url->nodeValue == $base_url) {	//the url has been taken, then take the nid
			$b_url_parent = $b_url->parentNode;
			$this_nid = $b_url_parent->getElementsByTagName('nid')->item(0)->nodeValue;
			if ($this_nid == $nid) {
				if ($b_url_parent->nodeName == "master") {
					$recreate = true;
				}
				
				$dumby = $b_url_parent->parentNode->removeChild($b_url_parent);
				
				if (isset($recreate)) {
					$master_rel = $dom->createElement("master");
					$dom->firstChild->appendChild($master_rel);
					unset($recreate);
				}
				
				break;
			}
		}
	}

	module_load_include('inc', 'fedora_repository', 'api/fedora_item');
	$fedora_object = new Fedora_Item($pid);

	global $user;
	$old_user = $user;
	$user = user_load(1);

	if ($fedora_object->modify_datastream_by_value($dom->saveXML(), $drupal_dsID, "Fedora Object to Druapl relationship", 'text/xml') !== NULL) {
		sleep(1);
		$user = $old_user;
		return true;
	}
	else {
		$user = $old_user;
		return false;
	}
}


/**
 * TODO questa è specifica per il datastream mag... bisognerebbe spostarlo da qui
 * o something else
 *
 * Take a xml string and retrieve an array like $form_values for MAG datasteam
 * @param string $xml
 */
function __mag_xml_to_array($xml_str) {
	$xml_multiarray_values = xml2array($xml_str);

	$xml_array_values = array();

	//this is needed because all "dc:" field must haven't a prefix
	$array_minus_bib = array_key_remove($xml_multiarray_values, "bib");
	$array_flatten_key_dc = array_flatten($xml_multiarray_values['metadigit']["bib"]);

	$array_flatten_key_prefix = array_flatten_sep($array_minus_bib['metadigit'], ":", "mag:");

	$xml_array_values = array_merge($array_flatten_key_dc, $array_flatten_key_prefix);

	$default_lang = variable_get("islandora_mag_default_metadigit_lang", "it");

	$xml_array_values["metadigit_lang"] = isset($xml_multiarray_values["metadigit_attr"]["xml:lang"]) ? $xml_multiarray_values["metadigit_attr"]["xml:lang"] : $default_lang;

	return $xml_array_values;
}

/**
 * Walks through a multidimensional array and takes only leafs with right keys
 *
 * @param array $array
 * 		multidimensional array
 * @param string $sep
 * 		key separator
 * @param string $pre
 * 		optional key prefix
 * @return array $return
 * 		monodimensional array
 */
function array_flatten_sep($array, $sep, $pre = "") {
  $result = array();
  $stack = array();
  array_push($stack, array("", $array));

  while (count($stack) > 0) {
    list($prefix, $array) = array_pop($stack);

    foreach ($array as $key => $value) {
      $new_key = $prefix . strval($key);

      if (is_array($value))
        array_push($stack, array($new_key . $sep, $value));
      else
        $result[$pre . $new_key] = $value;
    }
  }

  return $result;
}

/**
 * Flattens an array, or returns FALSE on fail.
 *
 * @param array $array
 */
function array_flatten($array) {
  if (!is_array($array)) {
    return FALSE;
  }
  $result = array();
  foreach ($array as $key => $value) {
  	//necessary because dc:subject can be an array
    if (is_array($value) and ($key != "dc:subject")) {
      $result = array_merge($result, array_flatten($value));
    }
    else {
      $result[$key] = $value;
    }
  }
  return $result;
}

/**
 * Remove a portion of a n-dimentional array based on the value of the key
 * @param unknown_type $array
 * @param unknown_type $key
 */
function array_key_remove($array, $key) {
	$holding = array();

	foreach($array as $k => $v){
		if (is_array($v) and $key != $k) {
			$holding [$k] = array_key_remove($v, $key);
		}
		elseif ($key != $k){
			$holding[$k] = $v; // removes an item by combing through the array in order and saving the good stuff
		}
	}
	return $holding; // only pass back the holding array if we didn't find the value
}


/**
 * xml2array() will convert the given XML text to an array in the XML structure.
 * Link: http://www.bin-co.com/php/scripts/xml2array/
 * Arguments : $contents - The XML text
 *                $get_attributes - 1 or 0. If this is 1 the function will get the attributes as well as the tag values - this results in a different array structure in the return value.
 *                $priority - Can be 'tag' or 'attribute'. This will change the way the resulting array sturcture. For 'tag', the tags are given more importance.
 * Return: The parsed XML in an array form. Use print_r() to see the resulting array structure.
 */
function xml2array($contents, $get_attributes=1, $priority = 'tag') {
	if(!$contents) return array();

	if(!function_exists('xml_parser_create')) {
		//print "'xml_parser_create()' function not found!";
		return array();
	}

	//Get the XML parser of PHP - PHP must have this module for the parser to work
	$parser = xml_parser_create('');
	xml_parser_set_option($parser, XML_OPTION_TARGET_ENCODING, "UTF-8"); # http://minutillo.com/steve/weblog/2004/6/17/php-xml-and-character-encodings-a-tale-of-sadness-rage-and-data-loss
	xml_parser_set_option($parser, XML_OPTION_CASE_FOLDING, 0);
	xml_parser_set_option($parser, XML_OPTION_SKIP_WHITE, 1);
	xml_parse_into_struct($parser, trim($contents), $xml_values);
	xml_parser_free($parser);

	if(!$xml_values) return;//Hmm...

	//Initializations
	$xml_array = array();
	$parents = array();
	$opened_tags = array();
	$arr = array();

	$current = &$xml_array; //Refference

	//Go through the tags.
	$repeated_tag_index = array();//Multiple tags with same name will be turned into an array
	foreach($xml_values as $data) {
		unset($attributes, $value);//Remove existing values, or there will be trouble

		//This command will extract these variables into the foreach scope
		// tag(string), type(string), level(int), attributes(array).
		extract($data);//We could use the array by itself, but this cooler.

		$result = array();
		$attributes_data = array();

		if(isset($value)) {
			if($priority == 'tag')
				$result = $value;
			else
				$result['value'] = $value; //Put the value in a assoc array if we are in the 'Attribute' mode
		}

		//Set the attributes too.
		if(isset($attributes) and $get_attributes) {
			foreach($attributes as $attr => $val) {
				if($priority == 'tag')
					$attributes_data[$attr] = $val;
				else
					$result['attr'][$attr] = $val; //Set all the attributes in a array called 'attr'
			}
		}

		//See tag status and do the needed.
		if($type == "open") { //The starting of the tag '<tag>'
			$parent[$level-1] = &$current;
			if(!is_array($current) or (!in_array($tag, array_keys($current)))) { //Insert New tag
				$current[$tag] = $result;
				if($attributes_data) $current[$tag. '_attr'] = $attributes_data;
				$repeated_tag_index[$tag.'_'.$level] = 1;

				$current = &$current[$tag];

			}
			else { //There was another element with the same tag name
				if(isset($current[$tag][0])) { //If there is a 0th element it is already an array
					$current[$tag][$repeated_tag_index[$tag.'_'.$level]] = $result;
					$repeated_tag_index[$tag.'_'.$level]++;
				}
				else { //This section will make the value an array if multiple tags with the same name appear together
					$current[$tag] = array($current[$tag],$result);//This will combine the existing item and the new item together to make an array
					$repeated_tag_index[$tag.'_'.$level] = 2;

					if(isset($current[$tag.'_attr'])) { //The attribute of the last(0th) tag must be moved as well
						$current[$tag]['0_attr'] = $current[$tag.'_attr'];
						unset($current[$tag.'_attr']);
					}

				}
				$last_item_index = $repeated_tag_index[$tag.'_'.$level]-1;
				$current = &$current[$tag][$last_item_index];
			}
		}
		elseif($type == "complete") { //Tags that ends in 1 line '<tag />'
			//See if the key is already taken.
			if(!isset($current[$tag])) { //New Key
				$current[$tag] = $result;
				$repeated_tag_index[$tag.'_'.$level] = 1;
				if($priority == 'tag' and $attributes_data) $current[$tag. '_attr'] = $attributes_data;

			}
			else { //If taken, put all things inside a list(array)
				if(isset($current[$tag][0]) and is_array($current[$tag])) {//If it is already an array...

					// ...push the new element into that array.
					$current[$tag][$repeated_tag_index[$tag.'_'.$level]] = $result;

					if($priority == 'tag' and $get_attributes and $attributes_data) {
						$current[$tag][$repeated_tag_index[$tag.'_'.$level] . '_attr'] = $attributes_data;
					}
					$repeated_tag_index[$tag.'_'.$level]++;

				}
				else { //If it is not an array...
					$current[$tag] = array($current[$tag],$result); //...Make it an array using using the existing value and the new value
					$repeated_tag_index[$tag.'_'.$level] = 1;

					if($priority == 'tag' and $get_attributes) {
						if(isset($current[$tag.'_attr'])) { //The attribute of the last(0th) tag must be moved as well

							try {
								$current[$tag]['0_attr'] = $current[$tag.'_attr'];
								unset($current[$tag.'_attr']);
							}
							catch (exception $e) {
								drupal_set_message(t('Error ') . $e->getMessage(), 'error');
							}


						}

						if($attributes_data) {
							$current[$tag][$repeated_tag_index[$tag.'_'.$level] . '_attr'] = $attributes_data;
						}
					}
					$repeated_tag_index[$tag.'_'.$level]++; //0 and 1 index is already taken
				}
			}

		}
		elseif($type == 'close') { //End of tag '</tag>'
			$current = &$parent[$level-1];
		}
	}

	return($xml_array);
}



/**
 * Returns the objects of a certain model.
 *
 * @param string $cm_pid
 * @param string $query_string - an alternative itql query
 */
function __getObjects($cm_pid, $query_string="") {
	module_load_include('inc', 'fedora_repository', 'api/fedora_utils');

	if (empty($query_string)) {
		$query_string = '
			select
				$title $identifier $modified
			from
				<#ri>
			where
				$object <dc:title> $title
				  and
		    $object <dc:identifier> $identifier
		  and
	    	$object <fedora-view:lastModifiedDate> $modified
			and
				$object <fedora-model:hasModel> <info:fedora/' . $cm_pid . '>
			order by $modified ';
	}

	$query_string = htmlentities(urlencode($query_string));

	$url = variable_get('fedora_repository_url', 'http://localhost:8080/fedora/risearch');
  	$url.= "?type=tuples&flush=TRUE&format=Sparql&limit=&offset=0&lang=itql&stream=on&query=" . $query_string;
  	$content = do_curl($url);

    if (empty($content)) {
      return NULL;
    }

	$items = new SimpleXMLElement( $content );

	if ( count( $items->results->result ) > 0 ) {
		foreach ( $items->results->result as $res ) {
			$objects[] =  (array) $res;
		}
	}

  	return $objects;
}

/**
 * Grabs content models related to the collection specified in the
 * configuration settings of this module.
 *
 * @return an array of content models
 */
function __getContentModels() {
	global $our_content_models;

	module_load_include('inc', 'fedora_repository', 'ContentModel');
	module_load_include('inc', 'fedora_repository', 'CollectionClass');

	$options = array();
	$collectionHelper = new CollectionClass();

	//defined in fedora_repository admin config
	$default_collection = variable_get('fedora_content_model_collection_pid','islandora:ContentModelCollection');

	$results = $collectionHelper->getRelatedItems( $default_collection,	null, null );

    if (empty($results)) {
      return NULL;
    }

	$items = new SimpleXMLElement( $results );

	if ( count( $items->results->result ) > 0 ) {
		foreach ( $items->results->result as $res ) {
			$child_pid = substr( $res->object['uri'], strpos($res->object['uri'],'/')+1 );
			if (in_array($child_pid, $our_content_models)):
				if ( ( $cm = ContentModel::loadFromModel( $child_pid ) ) !== false) {
					$options[$child_pid] = $child_pid;
				}
			endif;
		}
	}

	return $options;
}

/**
 * Get elements from a Content Model
 *
 * @param string $content_model
 */
function __getFormElements($content_model) {
	module_load_include('inc', 'fedora_repository', 'ContentModel');
	$form_elements = array();

  if ($cm = ContentModel::loadFromModel($content_model)) {
    if (($elements = $cm->getIngestFormElements()) !== false) {
      foreach ($elements as $element) {
      	//our elements are created in this way: metadigit][section][field
      	//we need to take only the "field" part
      	$name = explode('][', $element['name']);
      	$form_elements[$name[2]] = $name[2];
      }
    }
  }



  return $form_elements;
}

/**
 * Returns the machine name of the node type
 *
 * @param string $node_type_name
 * 		Human readable name of the node type
 */
function __getNodeTypeKey($node_type_name) {
	//key (machine) => value (human readable) array
	$node_types = node_get_types('names');

	//gets a key by value
  $node_type = array_search($node_type_name, $node_types);

  return $node_type;
}

/**
 * Returns the human readable name of the node type
 *
 * @param string $node_type_key
 * 		Machine readable name of the node type
 */
function __getNodeTypeName($node_type_key) {
	//key (machine) => value (human readable) array
	$node_types = node_get_types('names');

	//gets value, the human readable form
  $node_type = $node_types[$node_type_key];

  return $node_type;
}

/**
 * Get node type according to a content model
 * @param string $cm
 */
function __getNodeTypeAssoc($cm) {
	$nt = db_result(db_query("SELECT node_type FROM {islandora_sync_admin_type_assoc} WHERE content_model = '%s'", $cm));

	if ($nt != FALSE) {
	  $node_types = node_get_types('names');
  	foreach ($node_types as $key => $value) {
  		if ($value == $nt) {
  			return $key; //we need the key name, not the value
  		}
  	}
	}
	else {
		return FALSE;
	}
}

function __getCollectionPid($pid) {
    $uris = array();

  	module_load_include('inc', 'fedora_repository', 'ObjectHelper');
  	$object_helper = new ObjectHelper();
  	$collection_objs = $object_helper->get_parent_objects($pid);

  	try {
      $parent_collections = new SimpleXMLElement($collection_objs);
    }
    catch (exception $e) {
      drupal_set_message(t('Error getting parent objects !e', array('!e' => $e->getMessage())));
      return;
    }

    foreach ($parent_collections->results->result as $result) {
     foreach ($result->object->attributes() as $a => $b) {
        if ($a == 'uri') {
          $uri = (string) $b;
          $uri = substr($uri, strpos($uri, '/')+1);
        }
      }

      $uris[] = $uri;
    }

    return $uris;
  }

/**
 * Get collection TID by PID
 * @param string $pid - object pid
 */
function __getCollectionTidByPid( $pid ) {
	$tid = db_result(db_query("SELECT tid FROM {islandora_sync_pid_fpid_tid} WHERE pid = '%s'", $pid));

	return $tid;
}



function __showPagesPerBook($pid = "epistemetec:4845", $item_per_page = 9) {
	if (!isset($_GET['p'])) {
		$pagen = 1;
	}
	else {
		$pagen = $_GET['p'];
		$new_url = explode("?", request_uri());
		$new_url = $new_url[0];
	}
	
	$offset = $pagen * $item_per_page;
	
	module_load_include('inc', 'fedora_repository', 'api/fedora_utils');
	module_load_include('inc', 'fedora_repository', 'api/fedora_item');

	$DS_ID = 'TN';

	$itql = ' select $title $identifier from <#ri> ' .
		' where $object <dc:title> $title and $object <dc:identifier> $identifier ' .
		' and $object <info:fedora/fedora-system:def/relations-external#isMemberOf> <info:fedora/' . $pid . '>'.
		' order by $identifier';

	$query_string = htmlentities(urlencode($itql));
	
	$fedora_repository_url = variable_get('fedora_repository_url', 'http://localhost:8080/fedora/risearch');
  	$url = $fedora_repository_url . "?type=tuples&flush=TRUE&format=Sparql&limit=&offset=0&lang=itql&stream=on&query=" . $query_string;
  	$allcontent = do_curl($url);
  	$allitems = new SimpleXMLElement($allcontent);
	$total_n_of_items = count($allitems->results->result);
	$nofpages = $total_n_of_items / $item_per_page;
	

	$itql .= " limit $item_per_page offset $offset ";

	$query_string = htmlentities(urlencode($itql));
	
  	$url = $fedora_repository_url . "?type=tuples&flush=TRUE&format=Sparql&lang=itql&stream=on&query=" . $query_string;
  	echo $url;
  	$content = do_curl($url);
  
	if (empty($content)) {
		drupal_set_message(t('Error getting book pages !e', array('!e' => $e->getMessage())));
		return;
	}
	
	$items = new SimpleXMLElement($content);
	$n_of_items = count($items->results->result);
	$count_removed = 0;

	if ($n_of_items > 0) {
	    $output = '<div class="book-pages">';
		foreach ($items->results->result as $res) {
			$object = (array) $res;
			
			$pid = $object['identifier'];
			$pageid = explode("-", $pid);
			$pageid = $pageid[1];
			
            $output .= <<<HTML
                 <div class="book-page">
                    <img src="/fedora/repository/$pid/TN" class="book-page-image" />
			        <span class="book-page-title">Pag - $pageid</span>
			     </div>
HTML;
			
		}
		$output .= "</div><!-- /end book-pages-->";
                sleep(1);
	}
    else {
        $output = '<div class="book-pages">This book has not pages yet.</div>';
    }

    
    //display pager
    $i = 1;
    
    $output .= '<div class="book-pages-nav">';
    while ($i <= $nofpages) {
    	$class = $i == $pagen ? ' class="book-pages-nav-current-page"' : "";
    	$output .= '<a href="' . $new_url . "?p=" . $i . '" ' . $class .  '>' . $i . '</a> ';
    	
    	$i++;
    }
	$output .= "</div><!-- /end book-pages nav-->";

	echo $output;
	
}
