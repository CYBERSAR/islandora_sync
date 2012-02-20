<?php
	module_load_include('inc', 'fedora_repository', 'api/fedora_utils');
	module_load_include('inc', 'fedora_repository', 'api/fedora_item');

	$DS_ID = 'RELS_DRUPAL';

	$itql = 'select $title $identifier from <#ri> where $object <dc:title> $title and $object <dc:identifier> $identifier';

	$query_string = htmlentities(urlencode($itql));

	$url = variable_get('fedora_repository_url', 'http://localhost:8080/fedora/risearch');
  	$url.= "?type=tuples&flush=TRUE&format=Sparql&limit=&offset=0&lang=itql&stream=on&query=" . $query_string;
  	$content = do_curl($url);
  
	if (empty($content)) {
		echo "content is empty";
		exit (0);
	}
	
	$items = new SimpleXMLElement( $content );

	if ( count( $items->results->result ) > 0 ) {
		foreach ( $items->results->result as $res ) {
			$objects[] =  (array) $res;
		}
	}

	foreach ($objects as $object) {
		$pid = $object['identifier'];
		$fedora_item = new Fedora_Item($pid);
		$datastreams = $fedora_item->get_datastreams_list_as_array();

		if (isset($datastreams[$DS_ID])) {
			echo "purging DS Drupal Rel of object: $pid \n";
			//$fedora_item->purge_datastream($DS_ID);
		}
		else {
			echo "nothing to do with object: $pid\n";
		}

		//jhdskvnh
	}

?>
