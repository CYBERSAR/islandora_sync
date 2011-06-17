<?php
class MyNode {

	function MyNode() {
		drupal_bootstrap(DRUPAL_BOOTSTRAP_FULL);
	}

	function getNid($pid) {
		module_load_include('php', 'Fedora_Repository', 'ObjectHelper');
		//get Nid from Pid
		$object = new ObjectHelper($pid);
		$spec = $object->getStream($pid, 'RELS-DRUPAL',0);
		
		$xml = new SimpleXMLElement($spec);
		$nid = implode($xml->xpath('//nid'));
//		$urlNid = implode($xml->xpath('//nurl'));
//		$strNid = explode('node/', $urlNid);
//		$nid = $strNid[1];

		return $nid;
	}

	/** 
	 * Convert form_values keys to cck fields names.
	 * 
	 * @param object $node
	 * 		the node that will be created
	 * @param array $form_values
	 * 		values ingested
	 * @param string $type
	 * 		the node type in a machine readable form
	 * @param boolean $isEditing
	 * 		...?
	 */
	function __hashCCK(&$node, $form_values, $type, $isEditing = FALSE) {
		module_load_include('inc', 'islandora_sync', 'islandora_sync.admin');
		$node_type = __getNodeTypeName($type);
		
		$all_ccks = variable_get('islandora_sync_ccks', array());
		if (isset($all_ccks[$node_type])) {
			$type_ccks = $all_ccks[$node_type];
			
			foreach ($type_ccks as $cck => $value) {
				if (isset($form_values[$value])) {
					eval ("\$node->" . $cck . "[0]['value']=\"$form_values[$value]\";");
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
					eval ("\$node->" . $cck . "[0]['value']=\"$value\";");
				}
			}
		}
	}

	function createNode($form_values, $type, $print_message = true) {
		global $base_url;
		$nodeUrl = $base_url . '/fedora/repository/' . $form_values['pid'];
		
		// add node properties
		$newNode = (object) NULL;
		$newNode->type = $type;
		$newNode->title = $form_values['dc:title']; //TODO:handle on module configuration page
		$newNode->uid = 0;
		$newNode->created = strtotime("now");
		$newNode->changed = strtotime("now");
		$newNode->status = 1;
		$newNode->comment = 0;
		$newNode->promote = 0;
		$newNode->moderate = 0;
		$newNode->sticky = 0;
		
		/*
		 * We add CCK field data to the node. To be sure to use only valid ccks for this type of node 
		 * we extract all cck fields names using content_field api call. Then we execute the assignement
		 * where the $key from $ccks hash table is one of the cck fields.
		 */ 
		$this->__hashCCK($newNode, $form_values, $type);
		
		//TODO add islandora_sync_createnode_alter hook passando $newNode e $ccks
		
		//TODO spostare in islandora_mag invocando l'hook
		$newNode->body = "<a href=\"" . $nodeUrl . "\">" . $form_values['dc:title'] . "</a>";
		$newNode->field_fedora_thumbnail[0]['embed'] = $nodeUrl . "/TN";
		$newNode->field_fedora_thumbnail[0]['value'] = $nodeUrl . "/TN";
		$newNode->field_fedora_thumbnail[0]['provider'] = "custom_url";
		
		// save node		
		node_save($newNode); //TODO: manage exceptions
		
		$nid = trim($newNode->nid);
		
		if ($print_message)
			drupal_set_message(t('The Drupal node: @nid, was created successfully.', array('@nid' => $nid)));
		
		return $nid;
	}

	/**
	 * Update the CCK of the node when a digital object is modified.
	 * 
	 * @param array $form_values
	 * 		an array containing all edited values
	 * @param string $type
	 * 		the node type: this is used to know which CCK must be modified.
	 */
	function updateNode($form_values, $print_message = true) {
		global $base_url;
		$pid = $form_values['pid'];
		$nodeUrl = $base_url . '/fedora/repository/' . $pid;
		$nid = $this->getNid($pid);

		//load and edit 
		$node = node_load($nid);
		
		$this->__hashCCK($node, $form_values, $node->type);
		
		// save node	
		node_save($node);
		
		if ($print_message)
			drupal_set_message(t('The Drupal node: @nid, was updated successfully.', array('@nid' => $nid)));
	}
	
	function addNodeReference($ccks) {
		//print_r($ccks);exit;
		$pidParent = $ccks['field_fedora_collection_pid'];
		$nidParent = $this->getNid($pidParent);
		$pidChild = $ccks['field_fedora_pid'];
		//echo $pidChild; exit;
		$nidChild = $this->getNid($pidChild);
		
		//echo $nidChild; exit;
		//load and edit 
		try {
			$node = node_load($nidParent);
			$node->field_fedora_reference[sizeof($node->field_fedora_reference)] = array('nid'=>$nidChild);
			node_save($node);
		}
		
		catch (exception $e) {
			//node_delete($nid);
			drupal_set_message(t('Error Ingesting Object! ') . $e->getMessage(), 'error');
			watchdog(t("Fedora_Repository"), t("Error Ingesting Object!") . $e->getMessage(), NULL, WATCHDOG_ERROR);
			return;
		}
		//echo "<pre>"; var_dump($node);echo "</pre>";exit;
	}

	/**
	 * 
	 * Remove, if exist, dupal node 
	 * @param Drupal Node id $nid
	 */
	function deleteNode($nid) {
		//$nid = $this->getNid($pid);
		
		$node_exist = node_load($nid);
		if ($node_exist){
			node_delete($nid);
			drupal_set_message(t('The Drupal node: @nid, was deleted successfully.', array('@nid' => $nid)));
		}
	}
}
