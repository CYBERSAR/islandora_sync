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
        'islandora_sync_action_ontranslation' => array(
            'label' => t('Create Datastream On Translation'),
            'module' => 'Node',
            'arguments' => array(
                'node' => array(
                    'type' => 'node',
                    'label' => t('Content')
                )
            ),
        ),
    );
}

/**
 * From a translated node, creates an additional MAG datastream for a given
 * Fedora Object and update the DRUPAL_REL datastream to store this info.
 * 
 * What this function does is to take the global "$ccks" array (that was set
 * in the islandora_sync configuration panel) and for each one of them put
 * the value in the xml that will be saved as new object datastream.
 *
 * @param object $node - translation node
 */
function islandora_sync_action_ontranslation($node) {
    module_load_include('inc', 'fedora_repository', 'ObjectHelper');
    module_load_include('inc', 'fedora_repository', 'api/fedora_item');

    /*
     * 1. node type is checked with the rule's "IF" statement
     * 2. node exists because this function is invoked at node creation/publication,
     *    with the "ON" and "DO" statements rules
     * 3. pid exists because on object deletion there is an hook that remove related
     *    node (so it can't be translated and this function can't be invoked!)
     */

    $dom = new DomDocument("1.0", "UTF-8");
    $dom->formatOutput = TRUE;

    $ds_id = "MAG_" . strtoupper($node->language);
    $ds_label = "Mag Record ({$node->language})";
    $pid = $node->field_fedora_pid[0]['value'];

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

    module_load_include('php', 'islandora_sync', 'islandora_sync_utils');

    $sep = variable_get('islandora_sync_metadata_namespace_separator', ':');
    $prefix = variable_get('islandora_sync_fedora_cck_field_prefix', 'fedora_');
    $ccks = variable_get('islandora_sync_ccks', array());

    $node_type_name = __getNodeTypeName($node->type);
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
                        } else {
                            $holdings = $dom->createElement($tags[2]);
                            $bib->appendChild($holdings);
                        }
                        break;
                }
            }
        }
    }

    $fedora_item = new Fedora_Item($pid);
    $object_helper = new ObjectHelper();

    $drupal_info = $object_helper->getStream($pid, $ds_id);

    if (empty($drupal_info)) {
        $res = $fedora_item->add_datastream_from_string($dom->saveXML(), $ds_id, $ds_label, 'text/xml', 'X');
        
        if ($res !== NULL) {
            drupal_set_message("Added $ds_label successfully for $pid");
        }
        else {
            drupal_set_message("Can't add $ds_label for $pid", "error");
        }
    }
    else {
    	//here we can update the datastream if necessary
        drupal_set_message("Can't add $ds_label for $pid, DS already exists.", "warning");
    }


    return array('node' => $node);
}