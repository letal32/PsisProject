#ifndef API
#define API
#include <netinet/in.h>

/*
This function is called by the client before interacting with the system.
Arguments:
host and port arguments refer to the identification of the gateway server socket.

This function returns one integer corresponding to the socket opened between the client
and one of the peers.
The function return -1 if the gateway can not be accessed, 0 no peer is available or a
positive integer if a peer is accessible (this returned value is used in all other functions
(peer_socket)).
*/

int gallery_connect(char * host, in_port_t port);

/*
This function inserts a new photo into the system.
Arguments:
• peer_socket – this argument corresponds to the value returned by connect
• file_name – This argument corresponds to the name of the photo file (including
extension). The photos should be read from the program directory.
This function returns positive integer (corresponding to the photo identifier) or 0 in case of
error (Invalid file_name or problems in communication with the server).
*/

uint32_t gallery_add_photo(int peer_socket, char *file_name);

/*
This function add a new keyword to a photo already in the system.
Arguments:
• peer_socket – this argument corresponds to the value returned by connect
• file_name – This argument corresponds to the name of the photo file (including
extension)
*/

int gallery_add_keyword(int peer_socket, uint32_t id_photo, char *keyword);

/*
This function searches on the system for all the photos that contain the provided keyword.
The identifiers of the found photos will be stored in the array pointed by id_photos. This
argument will point to an array created inside the function using calloc. The length of the
id_photos array will returned by the function.
Arguments:
• peer_socket – this argument corresponds to the value returned by connect
• keyword – This argument corresponds to the keyword that will be used in the
search
• id_photos – The id of the photos will be stored in an array (created inside this
function). The id_photos parameter points to the variable where the address of this
array will be stored.
This function returns -1 positive integer (corresponding to number of found photos), 0 if no
photo contains the provided keyword, or a -1 if a error occurred (invalid arguments or
network problem.
*/

int gallery_search_photo(int peer_socket, char * keyword, uint32_t ** id_photos);

/*
This function removes from the system the photo identified by id_photo.
Arguments:
• peer_socket – this argument corresponds to the value returned by connect
• id_photo – This argument is the identifier of the photo to be removed from the
systemThis function returns 1 if the photo is removed successfully, 0 if the photo does not exist or
-1 in case of error
*/

int gallery_delete_photo(int peer_socket, uint32_t id_photo);

/*
This function retrieves from the system the name of the photo identified by id_photo.
Arguments:
• peer_socket – this argument corresponds to the value returned by connect
• id_photo – This argument is the identifier of the photo to be removed from the
system
• file_name – The name of the photo will be stored in an array (created inside this
function). The photo_name parameter points to the variable where the address of
this string will be stored
This function returns 1 if the photo existes in the system and the name was retrieved, 0 if
the photo does not exist or -1 in case of error.
*/

int gallery_get_photo_name(int peer_socket, uint32_t id_photo, char **photo_name);

/*
This function downloads from the system the photo identified by id_photo and stores it in
the file with the file_name name.
Arguments:
• peer_socket – this argument corresponds to the value returned by connect
• id_photo – This argument is the identifier of the photo to be removed from the
system
• file_name – This argument is the name of file that will contain the image
downloaded from the system
This function returns 1 if the photo is downloaded successfully, 0 if the photo does not
exist or -1 in case of error.
*/

int gallery_get_photo(int peer_socket, uint32_t id_photo, char *file_name);

#endif