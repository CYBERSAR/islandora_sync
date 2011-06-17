<?php
// $Id$

/**
* @file
* Administration page callbacks for the Islandora_Sync module.
*/

/**
* Form builder. Configure Islandora Sync Node type.
*
* @ingroup forms
* @see system_settings_form().
*/
function islandora_sync_admin_page() {
	drupal_set_title(t("Node Type to Content Model association"));

	$output = drupal_get_form('islandora_sync_admin_type_assoc');
	
  return $output;
}


function islandora_sync_admin_type_assoc(&$form_state) {
  $node_types = node_get_types('names');

  //TODO: manage node type adding/deleting
  /*
   * per l'aggiunta non c'è problema. per la cancellazione invece, si potrebbe fare così:
   * si leggono i tipi di nodo con node_get_types, però si controllano anche quelli presenti
   * nel db. se nel db ci sono nodi in più allora sono stati eliminati.
   * 
   * però la cosa deve essere automatica, quindi è probabile che ci si debba agganciare all'hook
   * dei tipi di nodo, e alla cancellazione cancellare anche dal db o chiedere cosa fare,insomma...
   */
  
  $content_models = __getContentModels();
  array_unshift($content_models, "---"); //prepend this empty value as default
  
  $form['node_to_cm'] = array('#tree' => TRUE);
  
  $i = 0;
  foreach ($node_types as $key => $value) {
  	$form['node_to_cm'][$i]['node_type'] = array(
    	'#type' => 'item',
    	'#value' => $value,
    );
    
    $default_cm = db_result(db_query("SELECT content_model FROM {islandora_sync_admin_type_assoc} WHERE node_type = '%s' ", $value));
    if (!$default_cm) {
    	$default_cm = "---";
    }
    
    $form['node_to_cm'][$i]['content_model'] = array(
    	'#type' => 'select',
    	'#options' => $content_models,
    	'#default_value' => $default_cm,
    );
    
    $i++;
  }
  
  $form['islandora_sync_update_nodes'] = array(
  	'#type' => 'checkbox',
  	'#title' => t('Nodify on save.'),
  	'#default_value' => variable_get('islandora_sync_update_nodes', FALSE),
  	'#description' => t('Check this to create/update nodes with the binding you\'re saving.'),
  );

  $form['submit'] = array(
  	'#type' => 'submit',
  	'#value' => t('Save configuration'),
  );
  
  
  $form['#theme'] = 'islandora_sync_admin_type_assoc';

  return $form;
}


/**
 * Form builder to define some useful variables
 */
function islandora_sync_admin_globals() {
  $form['islandora_sync_admin_globals'] = array(
    '#type' => 'fieldset',
    '#title' => t('Global settings'),
  );

  $form['islandora_sync_admin_globals']['islandora_sync_metadata_namespace_separator'] = array(
    '#type' => 'textfield',
    '#title' => t('Metadata namespace separator'),
    '#default_value' => variable_get('islandora_sync_metadata_namespace_separator', ':'),
    '#description' => t('Default separator used to split metadata fields.')
  );
  
  $form['islandora_sync_admin_globals']['islandora_sync_fedora_cck_field_prefix'] = array(
    '#type' => 'textfield',
    '#title' => t('CCK field prefix'),
    '#default_value' => variable_get('islandora_sync_fedora_cck_field_prefix', 'fedora_'),
    '#description' => t('Default prefix used to specify that a CCK is related to Fedora.')
  );
  
  $form['islandora_sync_admin_globals']['islandora_sync_item_dl'] = array(
    '#type' => 'textfield',
    '#title' => t('Item DL'),
    '#default_value' => variable_get('islandora_sync_item_dl', 'fedora_obj'),
    '#description' => t('...add a description here...')
  );
  
  $form['islandora_sync_admin_globals']['islandora_sync_collection_dl'] = array(
    '#type' => 'textfield',
    '#title' => t('Collection DL'),
    '#default_value' => variable_get('islandora_sync_collection_dl', 'fedora_collectiondl'),
    '#description' => t('...add a description here...')
  );
  
  $form['islandora_sync_admin_globals']['islandora_sync_default_dsid'] = array(
    '#type' => 'textfield',
    '#title' => t('Default dsID'),
    '#default_value' => variable_get('islandora_sync_default_dsid', 'MAG'),
    '#description' => t('Default datastream ID that will be used to retrieve objects information.')
  );
  
  $form['islandora_sync_admin_globals']['islandora_sync_drupal_dsid'] = array(
    '#type' => 'textfield',
    '#title' => t('Drupal dsID'),
    '#default_value' => variable_get('islandora_sync_drupal_dsid', 'RELS-DRUPAL'),
    '#description' => t('Default datastream ID that will be used to write Drupal Node information.')
  );
  
  $form['islandora_sync_admin_globals']['#submit'][] = 'islandora_sync_admin_globals_submit';
  

  return system_settings_form($form); 
}



/**
 * Process islandora_sync_admin_type_assoc form submission.
 */
function islandora_sync_admin_type_assoc_submit($form, &$form_state) {
	if ($form['islandora_sync_update_nodes']['#value']) {
		variable_set('islandora_sync_update_nodes', TRUE);
	}
	else {
		variable_set('islandora_sync_update_nodes', FALSE);
	}
	
	//save node_type to content_model association
	foreach (element_children($form['node_to_cm']) as $i) {
		$node = $form['node_to_cm'][$i]['node_type']['#value'];
		$cm = $form['node_to_cm'][$i]['content_model']['#value'];
		
		if (db_result(db_query("SELECT * FROM {islandora_sync_admin_type_assoc} WHERE node_type = '%s' ", $node))) {
			db_query("UPDATE {islandora_sync_admin_type_assoc} SET content_model = '%s' WHERE node_type = '%s' ", $cm, $node);
		}
		else {
			db_query("INSERT INTO {islandora_sync_admin_type_assoc} (content_model, node_type) VALUES('%s', '%s')",	$cm, $node);
		}
		
		if (variable_get('islandora_sync_update_nodes', FALSE) && $cm) {
			$cms[] = $cm;
		}
	}
	
	if (isset($cms)) {
		__nodify($cms);
	}
  
  drupal_set_message(t('Association settings has been saved.'));
  //cache_clear_all($form_state['values'] .':', 'cache_filter', TRUE);
}

function theme_islandora_sync_admin_type_assoc($form) {
	//die('<pre>' . print_r($form['node_to_cm'], TRUE) . '</pre>');
  $header = array(
  	array('data' => t('Node Type')),
  	array('data' => t('Content Model')),
  	array('data' => t('Operations'))
  );
  
  foreach (element_children($form['node_to_cm']) as $i) {
  	$rows[] = array(
    	drupal_render($form['node_to_cm'][$i]['node_type']),
    	drupal_render($form['node_to_cm'][$i]['content_model']),
    	l(t('edit'), 'admin/settings/islandora_sync/edit/' . $form['node_to_cm'][$i]['node_type']['#value'])
    );
  }

  $output = theme('table', $header, $rows);
  $output .= drupal_render($form);

  return $output;
}

/**
 * Creates or updates nodes for each (if there are) configured CM
 * 
 * @param array $cms - a list of Content Models
 */
function __nodify($cms) {
	$default_dsID = variable_get('islandora_sync_default_dsid', 'MAG');
	$drupal_dsID = variable_get('islandora_sync_drupal_dsid', 'RELS-DRUPAL');
	
	foreach ($cms as $cm) {		
		//search for all Fedora object with hasModel (RELS-EXT) equals to $cm
		$objects = __getObjects($cm);
		drupal_set_message(t('#@nofnodes Nodes for "@content_model" are saved.', array('@nofnodes' => count($objects), '@content_model' => $cm)));
		
		//for each object get its XML
		foreach ($objects as $object) {
			module_load_include('inc', 'fedora_repository', 'ObjectHelper');
			$objectHelper = new ObjectHelper();
			
			$ds_info = $objectHelper->getStream($object[object_pid], $default_dsID);
			//file_save_data($ds_info, 'nodify_debug.txt', FILE_EXISTS_RENAME);
			
			$xml_array_values = __mag_xml_to_array($ds_info); //TODO move it
			
			$xml_array_values['pid'] = $object[object_pid];
			
			//Drupal node has already been created?
			$drupal_info = $objectHelper->getStream($object[object_pid], $drupal_dsID);
			
			module_load_include('php', 'islandora_sync', 'mynode');
			$node = new MyNode();
			
			if (isset($drupal_info)) { //update
				$node->updateNode($xml_array_values, false);
			}
			else {
				//create the node
				$nid = $node->createNode($xml_array_values, __getNodeTypeAssoc($cm), false);
				
				//create the Res-Drupal datastream
				$dom = new DomDocument("1.0", "UTF-8");
				$dom->formatOutput = TRUE;
				$pid = $object[object_pid];
				$rootElement = $dom->createElement("foxml:digitalObject");
				$rootElement->setAttribute('VERSION', '1.1');
				$rootElement->setAttribute('PID', "$pid");
				$rootElement->setAttribute('xmlns:foxml', "info:fedora/fedora-system:def/foxml#");
				$rootElement->setAttribute('xmlns:xsi', "http://www.w3.org/2001/XMLSchema-instance");
				$rootElement->setAttribute('xsi:schemaLocation', "info:fedora/fedora-system:def/foxml# http://www.fedora.info/definitions/1/0/foxml1-1.xsd");
				$dom->appendChild($rootElement);
				
				$datastream = __create_drupal_datastream($dom, $rootElement, $nid);
				
				//save Res-Drupal datastream
				module_load_include('inc', 'fedora_repository', 'api/fedora_item');
				$fedora_object = new Fedora_Item($pid);
				if ($fedora_object->add_datastream_from_string($dom->saveXML(), $drupal_dsID) !== NULL) {
					drupal_set_message(t('Il datastream ' . $drupal_dsID . ' per l\'oggetto "%pid" è stato aggiunto con successo.', array('%pid'=>$pid)));
				}
			}
		}		
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
    if (is_array($value)) {
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

							$current[$tag]['0_attr'] = $current[$tag.'_attr'];
							unset($current[$tag.'_attr']);
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
 */
function __getObjects($cm_pid) {
	module_load_include('inc', 'fedora_repository', 'api/fedora_utils');
	
	// http://docs.mulgara.org/tutorial/itqlbasics.html
	$query_string = '
		select
			$title $object
		from
			<#ri>
		where
			$object <dc:title> $title
		and
			$object <fedora-model:hasModel> <info:fedora/' . $cm_pid . '>';
	$query_string = htmlentities(urlencode($query_string));

	$url = variable_get('fedora_repository_url', 'http://localhost:8080/fedora/risearch');
  $url.= "?type=tuples&flush=TRUE&format=CSV&limit=1000&offset=0&lang=itql&stream=on&query=" . $query_string;
  $content = do_curl($url);

  $rows = explode("\n", $content);
  
  array_shift($rows); // Knock of the first heading row of the csv

  $objects = array();
  if (count($rows)) {
    foreach ($rows as $row) {
      if ($row == "") {
        continue;
      }
      $fields = explode(',', $row);
      $object_pid = explode('/', $fields[1]);
      $objects[] = array('title' => $fields[0], 'object_pid' => $object_pid[1]);
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
	module_load_include('inc', 'fedora_repository', 'ContentModel');
	module_load_include('inc', 'fedora_repository', 'CollectionClass');

	$options = array();
	$collectionHelper = new CollectionClass();
	$items = new SimpleXMLElement(
		$collectionHelper->getRelatedItems(
			//defined in fedora_repository admin config
			variable_get('fedora_content_model_collection_pid','islandora:ContentModelCollection'),
			null,
			null
		)
	);

	if (count($items->results->result) > 0) {
		foreach ($items->results->result as $res) {
			$child_pid = substr($res->object['uri'], strpos($res->object['uri'],'/')+1);
			if (($cm = ContentModel::loadFromModel($child_pid))!==false) {
				$options[$child_pid] = $child_pid;
			}
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
 * Creates the configuration form to let associate CCKs with 
 * @param array $form_state
 * @param string $node
 * 		Human readable node type name
 */
function islandora_sync_cck_edit_form(&$form_state, $node) {
	$separator = variable_get('islandora_sync_metadata_namespace_separator', ':');
  $prefix = variable_get('islandora_sync_fedora_cck_field_prefix', 'fedora_');
  $ccks = variable_get('islandora_sync_ccks', array());
  
	//check if there is an association with node and content model
  if ($cm = db_result(db_query("SELECT content_model FROM {islandora_sync_admin_type_assoc} WHERE node_type = '%s'", $node))) {
  	$islandora_sync_node_type = variable_get('islandora_sync_node_type', 1);
  	
  	//gets cck (fields) from a node type
  	$type = content_types(__getNodeTypeKey($node));
  	$fields = $type['fields'];


  	$cm_form_elements = __getFormElements($cm);
  	array_unshift($cm_form_elements, "---"); //prepend this empty value as default	

  	
  	$form['#id'] = 'islandora_sync_node_field';
  	$form['#node_type'] = $node;

  	foreach ($fields as $field) {
  		$field_name = $field['field_name'];
  		$field_name_pieces = explode("_", $field_name, "3"); // 1 is for "field_", 2 is for "fedora_" and 3 is for the field name

  		$default_cm = 0;
  		if ($field_name_pieces[1] == substr($prefix, 0, -1)) {
  			$pippo = $ccks[$node][$field_name];
  			
  		  if (isset($ccks[$node][$field_name])) {
	  			$default_cm = $ccks[$node][$field_name];
	  		}
	  		else {
	  			foreach ($cm_form_elements as $key => $cm_form_element) {
	  				if (strcmp(str_replace($separator, "_", $cm_form_element), $field_name_pieces[2]) == 0) {
	  					$default_cm = $cm_form_element;
	  					break;
	  				}
	  			}
	  		}
	  		
	  		$form['islandora_sync_node_field']['field'][$field_name] = array(
		    	'#type' => 'select',
	  			'#title' => $field_name,
		    	'#options' => $cm_form_elements,
		    	'#default_value' => $default_cm,
	  		);
  		}
  	}

  	$form['islandora_sync_node_field']['submit'] = array(
		  '#type' => 'submit',
		  '#value' => t('Submit'),
  	);

  	$form['islandora_sync_node_field']['cancel'] = array(
		  '#type' => 'markup',
		  '#value' => l(t('Cancel'), 'admin/settings/islandora_sync'),
  	);

    return $form;
  }
  else {
    drupal_set_message(t('You have to select a Content Model with this Node Type.'), 'warning');
    
    $form['islandora_sync_node_field']['cancel'] = array(
		  '#type' => 'markup',
		  '#value' => l(t('<< Back'), 'admin/settings/islandora_sync'),
  	);

    return $form;
  }
}


function islandora_sync_cck_edit_form_submit($form, &$form_state) {
	$node_type = $form['#node_type'];
	
	$ccks = variable_get('islandora_sync_ccks', array());
	foreach ($form['islandora_sync_node_field']['field'] as $field) {
		$cck_name = $field['#title'];
		$metadata_field = $field['#value'];
		
		$ccks[$node_type][$cck_name] = $metadata_field;
	}
	
	variable_set('islandora_sync_ccks', $ccks);
	
	drupal_set_message(t('CCK to Content Model association settings has been saved.'));
}