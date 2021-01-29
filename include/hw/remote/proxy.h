/*
 * Copyright © 2018, 2021 Oracle and/or its affiliates.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef PROXY_H
#define PROXY_H

#include "hw/pci/pci.h"
#include "io/channel.h"

#define TYPE_PCI_PROXY_DEV "x-pci-proxy-dev"
OBJECT_DECLARE_SIMPLE_TYPE(PCIProxyDev, PCI_PROXY_DEV)

struct PCIProxyDev {
    PCIDevice parent_dev;
    char *fd;

    /*
     * Mutex used to protect the QIOChannel fd from
     * the concurrent access by the VCPUs since proxy
     * blocks while awaiting for the replies from the
     * process remote.
     */
    QemuMutex io_mutex;
    QIOChannel *ioc;
    Error *migration_blocker;
};

#endif /* PROXY_H */
