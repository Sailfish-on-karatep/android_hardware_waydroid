/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * sw_sync for vendor builds.
 *
 * hwcomposer.cpp drives its retire fences off a sw_sync timeline, but the
 * sw_sync_* symbols cannot be linked from libsync in a vendor module: this is
 * built as a vendor variant, so it resolves libsync through the LLNDK stub,
 * whose symbol map (system/core/libsync/libsync.map.txt) exports only the
 * sync_* API. sw_sync_* are platform-private, and linking fails with
 *
 *   ld.lld: error: undefined symbol: sw_sync_timeline_create
 *
 * The functions are three thin ioctl wrappers, so they are provided here
 * rather than by widening the platform's LLNDK surface -- vendor code has no
 * business reaching into platform-private symbols, and patching AOSP to let it
 * would be the wrong direction.
 *
 * Behaviour is identical to system/core/libsync/sync.c, including the ioctl
 * numbers and the /sys/kernel/debug/sync/sw_sync fallback ordering, so this is
 * a drop-in for what the stock Waydroid hwcomposer resolves at runtime.
 *
 * Declarations come from <libsync/sw_sync.h>, which the module already picks up
 * via its system/core include dir.
 */

#include <fcntl.h>
#include <linux/types.h>
#include <string.h>
#include <sys/ioctl.h>

#include <libsync/sw_sync.h>

struct sw_sync_create_fence_data {
    __u32 value;
    char name[32];
    __s32 fence;
};

#define SW_SYNC_IOC_MAGIC 'W'
#define SW_SYNC_IOC_CREATE_FENCE _IOWR(SW_SYNC_IOC_MAGIC, 0, struct sw_sync_create_fence_data)
#define SW_SYNC_IOC_INC _IOW(SW_SYNC_IOC_MAGIC, 1, __u32)

int sw_sync_timeline_create(void)
{
    int ret;

    ret = open("/sys/kernel/debug/sync/sw_sync", O_RDWR);
    if (ret < 0)
        ret = open("/dev/sw_sync", O_RDWR);

    return ret;
}

int sw_sync_timeline_inc(int fd, unsigned count)
{
    __u32 arg = count;

    return ioctl(fd, SW_SYNC_IOC_INC, &arg);
}

int sw_sync_fence_create(int fd, const char *name, unsigned value)
{
    struct sw_sync_create_fence_data data;
    int err;

    data.value = value;
    strlcpy(data.name, name, sizeof(data.name));

    err = ioctl(fd, SW_SYNC_IOC_CREATE_FENCE, &data);
    if (err < 0)
        return err;

    return data.fence;
}
