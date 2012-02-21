<?php
	module_load_include('inc', 'fedora_repository', 'api/fedora_utils');
	module_load_include('inc', 'fedora_repository', 'api/fedora_item');

	$DS_ID = 'RELS-DRUPAL';

	$itql = 'select $title $identifier from <#ri> where $object <dc:title> $title and $object <dc:identifier> $identifier
		and
		( $object <fedora-model:hasModel> <info:fedora/epistemetec:mag_img> or
		  $object <fedora-model:hasModel> <info:fedora/epistemetec:mag_book> or
		  $object <fedora-model:hasModel> <info:fedora/epistemetec:mag_big_img> or
		  $object <fedora-model:hasModel> <info:fedora/epistemetec:mag_video> or
		  $object <fedora-model:hasModel> <info:fedora/epistemetec:mag_audio> )';

	$query_string = htmlentities(urlencode($itql));

	$url = variable_get('fedora_repository_url', 'http://localhost:8080/fedora/risearch');
	echo "Fedora Repository URL: $url\n";
	echo "NB: each dot will be an object excluded from the deletion of the Drupal Rel\n\n";

  	$url.= "?type=tuples&flush=TRUE&format=Sparql&limit=&offset=0&lang=itql&stream=on&query=" . $query_string;
  	$content = do_curl($url);
  
	if (empty($content)) {
		echo "content is empty... check Fedora Repository connection.\n\n";
		exit (0);
	}
	
	$items = new SimpleXMLElement( $content );

	if ( count( $items->results->result ) > 0 ) {
		foreach ( $items->results->result as $res ) {
			$object = (array) $res;

			$pid = $object['identifier'];
			$fedora_item = new Fedora_Item($pid);
			$datastreams = $fedora_item->get_datastreams_list_as_array();

			if (isset($datastreams[$DS_ID])) {
				echo " [ purging DS Drupal Rel of object: $pid ]\n";
				$fedora_item->purge_datastream($DS_ID);
			}
			else {
				echo ".";
			}

		}
	}
?>
