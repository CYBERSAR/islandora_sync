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
  
  $form['islandora_sync_admin_globals']['#submit'][] = 'islandora_sync_admin_globals_submit';

  return system_settings_form($form); 
}



/**
 * Process islandora_sync_admin_type_assoc form submission.
 */
function islandora_sync_admin_type_assoc_submit($form, &$form_state) {
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