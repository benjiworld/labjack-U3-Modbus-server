#include "labjackusb.h"
#include "u3.h"
#include <stdint.h>

/*
 * Compatibility wrappers used by modbus_u3.c.
 *
 * The U3 example API provides eDI() and eDO(), but this Modbus mapping
 * uses generic eGet() and ePut() calls for digital I/O.  These wrappers
 * translate the required calls into the official U3 easy functions.
 */

/* Must match the values used by modbus_u3.c. */
#define LJ_ioPUT_DIGITAL_CHANNEL  3
#define LJ_ioGET_DIGITAL_CHANNEL  4

/*
 * Open the first available U3.
 *
 * openUSBConnection() opens a U3 selected by Local ID.  Passing -1
 * requests the first available U3.
 */
long eOpenU3(HANDLE *h)
{
    HANDLE handle;

    if (h == NULL)
        return -1;

    handle = openUSBConnection(-1);
    if (handle == NULL)
        return -1;

    *h = handle;
    return 0;
}

/* Close a U3 handle opened by eOpenU3(). */
long eClose(HANDLE h)
{
    if (h == NULL)
        return -1;

    closeUSBConnection(h);
    return 0;
}

/*
 * Read one digital channel.
 *
 * ConfigIO = 1 makes eDI() configure FIO/EIO channels as digital
 * automatically where appropriate.
 */
long eGet(HANDLE h, long ioType, long channel, long x1, long *x2)
{
    long state;
    long r;

    (void)x1;

    if (h == NULL || x2 == NULL)
        return -1;

    if (ioType != LJ_ioGET_DIGITAL_CHANNEL)
        return -1;

    if (channel < 0 || channel > 19)
        return -1;

    state = 0;

    r = eDI(h, 1, channel, &state);
    if (r != 0)
        return r;

    *x2 = state ? 1 : 0;
    return 0;
}

/*
 * Write one digital channel.
 *
 * ConfigIO = 1 makes eDO() configure FIO/EIO channels as digital
 * automatically where appropriate.
 */
long ePut(HANDLE h, long ioType, long channel, long x1, long *x2)
{
    long r;

    (void)x2;

    if (h == NULL)
        return -1;

    if (ioType != LJ_ioPUT_DIGITAL_CHANNEL)
        return -1;

    if (channel < 0 || channel > 19)
        return -1;

    r = eDO(h, 1, channel, x1 ? 1 : 0);
    return r;
}

