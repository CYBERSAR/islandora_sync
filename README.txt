Main settings
-------------

First of all you must create content types (CT) that you want associate with Fedora content models (CM).
You can find our content type exports into the folder named "Drupal-Content-Types".
Once imported, to enable multilingual support, go to each CT edit and enable it in the Workflow settings ("Impostazione del flusso di lavoro" in italian).


Next, you need to manually associate one CT to the corresponding CM. To do that go into "admin/settings/islandora_sync"
and click on "Salva configurazione" to save the configuration.

Into the "Operazioni" column you can click on "Modifica" to modify the mapping of each CCK to the related CM's fields.

Once you have done, you can set some useful variables going to "/admin/settings/islandora_sync/settings".

To create nodes, just click on the "Cron" settings: in this way 2 callbacks will be fired up. One to retrieve all
objects and create a FIFO table. A second one to pop N rows from this table and to create nodes.



Translation settings
--------------------

Into the folder "Rules" there is a file with the export of the used rules.
To use it you need to enable "rules" and "rules_admin" modules and then use the import/export function to import them.
Note: CT translation must be enabled. See "Main settings". Go to "admin/content/translation-management/icl-check" to verify CTs with translation enabled.
