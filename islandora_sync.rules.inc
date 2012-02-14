<?php

/*
 * Docs:
 * -----
 * http://drupal.org/documentation/modules/rules
 * http://dominiquedecooman.com/blog/drupal-custom-rules-how-write-your-own-events-conditions-actions-and-custom-object-custom-token
 * http://drupal.org/node/299055
 */

/**
 * Implementation of hook_rules_action_info().
 */
function islandora_sync_rules_action_info() {
    return array(
        'islandora_sync_action_on_translation_created' => array(
            'label' => t('Add FO-datastream when the node-translation is created'),
            'module' => t('Islandora Sync'),
            'arguments' => array(
                'node' => array(
                    'type' => 'node',
                    'label' => t('Content')
                )
            ),
        ),
        'islandora_sync_action_on_translation_deleted' => array(
            'label' => t('Remove the FO-datastream when the node-translation is deleted'),
            'module' => t('Islandora Sync'),
            'arguments' => array(
                'node' => array(
                    'type' => 'node',
                    'label' => t('Content')
                )
            ),
        )
    );
}

/**
 * From a translated node, create an additional MAG datastream for a given
 * Fedora Object and update the DRUPAL_REL datastream to store this info
 * 
 * Considerations:
 * 1. node type is checked with the rule "IF" statement
 * 2. node exists because this function is invoked at node creation/publication,
 *    with the "ON" and "DO" statements rules
 * 3. pid exists because on object deletion there is an hook that remove related
 *    node (so it can't be translated and this function can't be invoked!)
 *
 * @param object $node
 */
function islandora_sync_action_on_translation_created($node) {
	
	if (!isset($node->translation_source->nid)) {
		drupal_set_message("{$node->nid} is not a translation node: it's the main node", "notice"); //DEBUG
		return;
	}
	else {
		drupal_set_message("Creating node $node->translation_source->nid as translation of {$node->nid}", "notice"); //DEBUG
	}
	
    module_load_include('inc', 'fedora_repository', 'ObjectHelper');
    module_load_include('inc', 'fedora_repository', 'api/fedora_item');
    module_load_include('php', 'islandora_sync', 'islandora_sync_utils');

    $sep = variable_get('islandora_sync_metadata_namespace_separator', ':');
    $prefix = variable_get('islandora_sync_fedora_cck_field_prefix', 'fedora_');
    $ccks = variable_get('islandora_sync_ccks', array());
    $drupal_dsID = variable_get('islandora_sync_drupal_dsid', 'RELS-DRUPAL');
    
    $ds_id = "MAG_" . strtoupper($node->language);
    $ds_label = "Mag Record ({$node->language})";
    $pid = $node->field_fedora_pid[0]['value'];
    
    $fedora_item = new Fedora_Item($pid);
    $object_helper = new ObjectHelper();
    
    $drupal_info = $object_helper->getStream($pid, $ds_id);

    if ( !empty($drupal_info) ) {
    	//datastream already exists, so remove it and then re-create it
    	try {
    		$fedora_item->purge_datastream($ds_id);
    	}
    	catch (exception $e) {
    		drupal_set_message(t($e->getMessage()), 'error');
    	}
    }

    //create the xml structure
    $dom = new DomDocument("1.0", "UTF-8");
    $dom->formatOutput = TRUE;

    $rootElement = $dom->createElement("foxml:digitalObject");
    $rootElement->setAttribute('VERSION', '1.1');
    $rootElement->setAttribute('PID', "$pid");
    $rootElement->setAttribute('xmlns:foxml', "info:fedora/fedora-system:def/foxml#");
    $rootElement->setAttribute('xmlns:xsi', "http://www.w3.org/2001/XMLSchema-instance");
    $rootElement->setAttribute('xsi:schemaLocation', "info:fedora/fedora-system:def/foxml# http://www.fedora.info/definitions/1/0/foxml1-1.xsd");
    $dom->appendChild($rootElement);

    $datastream = $dom->createElement("foxml:datastream");
    $datastream->setAttribute("ID", $ds_id);
    $datastream->setAttribute("STATE", "A");
    $datastream->setAttribute("CONTROL_GROUP", "X");
    $rootElement->appendChild($datastream);

    $version = $dom->createElement("foxml:datastreamVersion");
    $version->setAttribute("ID", "MAG.0");
    $version->setAttribute("MIMETYPE", "text/xml");
    $version->setAttribute("LABEL", $ds_label);
    $datastream->appendChild($version);

    $content = $dom->createElement("foxml:xmlContent");
    $version->appendChild($content);

    $mag = $dom->createElement("metadigit");
    $mag->setAttribute("xmlns:dc", "http://purl.org/dc/elements/1.1/");
    $mag->setAttribute('xml:lang', $node->language);
    $content->appendChild($mag);

    $node_type_name = __getNodeTypeName($node->type);
     
    //$ccks is the key-value array created from the admin interface of islandora_sync
    //after Content Type to Fedora Object association
    foreach ($ccks[$node_type_name] as $cck => $mag_field) {
    	if (!empty($mag_field)) {
    		$value = $node->$cck;
    		$value = $value[0]['value'];

    		$test = substr($mag_field, 0, 2);

    		if ($test == 'ma' || $test == 'dc') {
    			$tags = explode(":", $mag_field);
    			$sw_value = $test == 'dc' ? $tags[0] : $tags[1];

    			switch ($sw_value) {

    				case "gen":
    					if (!isset($gen)) {
    						$gen = $dom->createElement("gen");
    						$mag->appendChild($gen);
    					}
    					 
    					$element = $dom->createElement($tags[2], $value);
    					$gen->appendChild($element);
    					 
    					break;

    				case "dc":
    					if (!isset($bib)) {
    						$bib = $dom->createElement("bib");
    						$mag->appendChild($bib);
    					}
    					 
    					$ele = $mag_field;
    					$element = $dom->createElement($ele, $value);
    					$bib->appendChild($element);

    					//UHM... questa parte serve? TODO
    					if ($ele == "dc:subject") {
    						$term = taxonomy_get_term_by_name($value);
    						$tid = $term[0]->tid;
    						$parents = taxonomy_get_parents($tid);
    						foreach ($parents as $parent) {
    							$element = $dom->createElement($ele, $parent->name);
    							$bib->appendChild($element);
    						}
    					}
    					 
    					break;

    				case "bib":
    					if (!isset($bib)) {
    						$bib = $dom->createElement("bib");
    						$mag->appendChild($bib);
    					}
    					 
    					if (isset($tags[3])) {
    						if (isset($holdings)) {
    							$element = $dom->createElement($tags[3], $value);
    							$holdings->appendChild($element);
    						}
    					}
    					else {
    						$holdings = $dom->createElement($tags[2]);
    						$bib->appendChild($holdings);
    					}
    					 
    					break;
    			}
    		}
    	}
    }

    unset($element);
     
    $res = $fedora_item->add_datastream_from_string($dom->saveXML(), $ds_id, $ds_label, 'text/xml', 'X');
    if ($res !== NULL) {
    	drupal_set_message("Added $ds_label successfully for $pid");
    }
    else {
    	drupal_set_message("Failed to add $ds_label for $pid", "error");
    }
    


    /* manage drupal-rel datastream to store translation information */

    if (isset($dom)) unset($dom);
    
    $drupal_info = $object_helper->getStream( $pid, $drupal_dsID );

    if ( !empty( $drupal_info ) ) {
      $dom = new SimpleXMLElement( $drupal_info );
      
      $element = __get_current_element($dom);

      if ( !empty( $element ) ) {
        if ( !($translations = $element->translations) ) {
          $translations = $element->addChild( 'translations' );
        }
        else {
        	foreach ($translations->translation as $translation) {
        		if ($translation->nid == $node->nid) {
        			$found = true;
        			break;
        		}
        	}
        	unset($translation);
        }

        if (!isset($found)) {
	        $translation = $translations->addChild( 'translation' );
	        $translation->addChild( 'nid', $node->nid );
	        $translation->addChild( 'lang', $node->language );
	        
	        if ($fedora_item->modify_datastream_by_value($dom->saveXML(), $drupal_dsID, "Fedora Object to Druapl relationship", 'text/xml') !== NULL) {
	        	drupal_set_message( "$drupal_dsID updated successfully for $ds_id" );
	        }
	        else {
	        	drupal_set_message( "Failed to update $drupal_dsID for $ds_id" );
	        }
        }
      }
      else {
      	drupal_set_message( "Failed to handle translation element on FO $pid." );
      }
    }
    else {
      drupal_set_message( "Failed to read $drupal_dsID on FO $pid." );
    }
    
    return array('node' => $node);
}

/**
 * purge datastream "MAG_%lang%" and remove relationship on RELS-DRUPAL
 * 
 * @param unknown_type $node
 */
function islandora_sync_action_on_translation_deleted($node) {

	//purge datastream
	$ds_id = "MAG_" . strtoupper($node->language);
	$pid = $node->field_fedora_pid[0]['value'];
	$nid = $node->nid;

	module_load_include('inc', 'fedora_repository', 'api/fedora_item');
	$fedora_item = new Fedora_Item($pid);

	try {
		$fedora_item->purge_datastream($ds_id);
	}
	catch (exception $e) {
		drupal_set_message(t($e->getMessage()), 'error');
		return array('node' => $node);
	}

	//remove relationship
	$drupal_dsID = variable_get('islandora_sync_drupal_dsid', 'RELS-DRUPAL');
	global $base_url;

	//load datastream
	module_load_include('inc', 'fedora_repository', 'ObjectHelper');
	$objectHelper = new ObjectHelper();

	$drupal_info = $objectHelper->getStream($pid, $drupal_dsID);
	if ( empty( $drupal_info ) ) {
		drupal_set_message( "Failed to load $drupal_dsID for object $pid", 'error' );
	}
	else {
		//retrieve translation elements
		$dom = new SimpleXMLElement( $drupal_info );
		$element = __get_current_element($dom);
	
		if ( !empty( $element ) ) {
			if ( $translations = $element->translations ) {
				foreach ($translations->translation as $translation) {
					if ($translation->nid == $nid) {
						unset($translations->translation); //remove this translation
						break;
					}
				}
			}
	
			//update the RELS-DRUPAL datastream
			module_load_include('inc', 'fedora_repository', 'api/fedora_item');
			$fedora_object = new Fedora_Item($pid);
	
			if ($fedora_object->modify_datastream_by_value($dom->saveXML(), $drupal_dsID, "Fedora Object to Druapl relationship", 'text/xml') !== NULL) {
				drupal_set_message( "Successfully updated $drupal_dsID for object $pid", 'notice' );
			}
			else {
				drupal_set_message( "Failed to update $drupal_dsID for object $pid", 'error' );
			}
		}
		else {
			drupal_set_message( "SimpleXML has failed...", 'error' );
		}
	}
	return array('node' => $node);
}


function __get_current_element( $dom ) {
	global $base_url;
	$is_master = variable_get("islandora_sync_is_master", 0);
	
	if ( $is_master == 1 ) {
		$element = $dom->master;
	}
	else {
		foreach ( $dom->slaves->slave as $slave ) {
			if ( $slave->base_url == $base_url ) { //got this slave
				/*
				* TODO qui tecnicamente c'è da controllare se esiste già la stanza relativa
				* al nodo traduzione ma se questo controllo si fa tramite l'if delle rules
				* allora qui risulterebbe superfluo.
				* si suppone che il controllo ci sia per il momento
				*/
				$element = $slave;
				break;
			}
		}
	}
	
	return isset($element) ? $element : null;
}




