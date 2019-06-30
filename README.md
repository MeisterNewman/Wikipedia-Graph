# Wikipedia-Graph
A program to map Wikipedia and manipulate the map.

To compile a map, the program needs a Wikipedia XML dump file.

Commands:

To build the graph:
wikigraph build "xml_dump_path" (-o "output_path")

To see some general statistics on a built graph (this will be fleshed out): 
wikigraph stats (-table "tablepath")

To see info regarding an individual page:
wikigraph stats "pagename" (-table "tablepath")

To find a shortest path between two pages:
wikigraph path "firstpage" "secondpage" (-table "tablepath")
