
First of all you must create content types (CT) that you want associate with Fedora content models (CM).
You can find our content type exports into the folder named "Drupal-Content-Types".

Next, you need to manually associate one CT to the corresponding CM. To do that go into "admin/settings/islandora_sync"
and click on "Salva configurazione" to save the configuration.

Into the "Operazioni" column you can click on "Modifica" to modify the mapping of each CCK to the related CM's fields.

Once you have done, you can set some useful variables going to "/admin/settings/islandora_sync/settings".

To create nodes, just click on the "Cron" settings: in this way 2 callbacks will be fired up. One to retrieve all
objects and create a FIFO table. A second one to pop N rows from this table and to create nodes.
