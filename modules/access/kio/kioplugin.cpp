#ifdef HAVE_CONFIG_H
# include "config.h"
#endif


/* VLC core API headers */
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>

/* KDE includes */
#include <kio/filejob.h>
#include <kio/job.h>

/* Own include */
#include "kiomediastream.h"

/* Forward declarations */
static int Open(vlc_object_t *);
static void Close(vlc_object_t *);
static int Control(access_t *, int i_query, va_list args);
static block_t Block(access_t *);

/* Module descriptor */
vlc_module_begin()
    set_shortname(N_("KIO"))
    set_description(N_("KIO access module"))
    set_capability("access", 60)
    set_callbacks(Open, Close)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)
vlc_module_end ()

/* Internal state for an instance of the module */
struct intf_sys_t
{
    KioMediaStream *instance = 0;
};

/**
 * Starts our example interface.
 */
static int Open(vlc_object_t *obj)
{
    intf_thread_t *intf = (intf_thread_t *)obj;

    /* Allocate internal state */
    intf_sys_t *sys = new intf_sys_t;
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    intf->p_sys = sys;

    msg_Info(intf, "lol kio!", who);
    return VLC_SUCCESS;

error:
    free(sys);
    return VLC_EGENERIC;    
}

/**
 * Stops the interface. 
 */
static void Close(vlc_object_t *obj)
{
    intf_thread_t *intf = (intf_thread_t *)obj;
    intf_sys_t *sys = intf->p_sys;

    msg_Info(intf, "Good bye %s!");

    /* Free internal state */
    free(sys->who);
    free(sys);
}

static int Seek(access_t *obj, uint64_t pos)
{
}

static int Control(access_t *obj, int query, va_list arguments)
{
}

static block_t *Block(access_t *obj)
{
}
