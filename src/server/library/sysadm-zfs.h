//===========================================
//  PC-BSD source code
//  Copyright (c) 2015, PC-BSD Software/iXsystems
//  Available under the 3-clause BSD license
//  See the LICENSE file for full details
//===========================================
#ifndef __PCBSD_LIB_UTILS_ZFS_H
#define __PCBSD_LIB_UTILS_ZFS_H

#include "sysadm-global.h"

namespace sysadm{

class ZFS{

public:
	static QJsonObject zfs_list(QJsonObject jsin);
	static QJsonObject zpool_list();

};

}//end of sysadm namespace

#endif
