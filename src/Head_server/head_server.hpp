#pragma once
#include <hiredis/hiredis.h>
#include <iostream>

// Types of requst:
//
//	 Get file
//	 Request File Deletion
//	 Health check
//	 Requst replication
//	 Adding a new cluster node / server
//	 Replace the zookeeper cluster with this server
//

enum request_type
{
        Get_file,
        Request_file_deletion,
        Health_check,
        Request_replication,
        New_cluster_server,
        Replace_zookeper,
        // TODO: Add more reuqest types in future

};

struct request
{
        request_type type;
};
