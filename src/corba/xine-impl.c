#include "xine.h"

/*** App-specific servant structures ***/
typedef struct
{
   POA_Xine servant;
   PortableServer_POA poa;

   CORBA_unsigned_short attr_position;

   Xine_PlayerStatus attr_status;

   CORBA_unsigned_short attr_audioChannel;

   CORBA_unsigned_short attr_spuChannel;

}
impl_POA_Xine;

/*** Implementation stub prototypes ***/
static void impl_Xine__destroy(impl_POA_Xine * servant,
			       CORBA_Environment * ev);
static void
impl_Xine_play(impl_POA_Xine * servant,
	       CORBA_char * MRL,
	       CORBA_unsigned_short position, CORBA_Environment * ev);

static void impl_Xine_pause(impl_POA_Xine * servant, CORBA_Environment * ev);

static void impl_Xine_stop(impl_POA_Xine * servant, CORBA_Environment * ev);

static CORBA_long
impl_Xine_eject(impl_POA_Xine * servant,
		CORBA_char * MRL, CORBA_Environment * ev);

static CORBA_unsigned_short
impl_Xine__get_position(impl_POA_Xine * servant, CORBA_Environment * ev);

static Xine_PlayerStatus
impl_Xine__get_status(impl_POA_Xine * servant, CORBA_Environment * ev);

static CORBA_unsigned_short
impl_Xine__get_audioChannel(impl_POA_Xine * servant, CORBA_Environment * ev);
static void
impl_Xine__set_audioChannel(impl_POA_Xine * servant,
			    CORBA_unsigned_short value,
			    CORBA_Environment * ev);

static CORBA_unsigned_short
impl_Xine__get_spuChannel(impl_POA_Xine * servant, CORBA_Environment * ev);
static void
impl_Xine__set_spuChannel(impl_POA_Xine * servant,
			  CORBA_unsigned_short value, CORBA_Environment * ev);

/*** epv structures ***/
static PortableServer_ServantBase__epv impl_Xine_base_epv = {
   NULL,			/* _private data */
   NULL,			/* finalize routine */
   NULL,			/* default_POA routine */
};
static POA_Xine__epv impl_Xine_epv = {
   NULL,			/* _private */
   (gpointer) & impl_Xine_play,

   (gpointer) & impl_Xine_pause,

   (gpointer) & impl_Xine_stop,

   (gpointer) & impl_Xine_eject,

   (gpointer) & impl_Xine__get_position,

   (gpointer) & impl_Xine__get_status,

   (gpointer) & impl_Xine__get_audioChannel,
   (gpointer) & impl_Xine__set_audioChannel,

   (gpointer) & impl_Xine__get_spuChannel,
   (gpointer) & impl_Xine__set_spuChannel,

};

/*** vepv structures ***/
static POA_Xine__vepv impl_Xine_vepv = {
   &impl_Xine_base_epv,
   &impl_Xine_epv,
};

/*** Stub implementations ***/
static Xine
impl_Xine__create(PortableServer_POA poa, CORBA_Environment * ev)
{
   Xine retval;
   impl_POA_Xine *newservant;
   PortableServer_ObjectId *objid;

   newservant = g_new0(impl_POA_Xine, 1);
   newservant->servant.vepv = &impl_Xine_vepv;
   newservant->poa = poa;
   POA_Xine__init((PortableServer_Servant) newservant, ev);
   objid = PortableServer_POA_activate_object(poa, newservant, ev);
   CORBA_free(objid);
   retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);

   return retval;
}

static void
impl_Xine__destroy(impl_POA_Xine * servant, CORBA_Environment * ev)
{
   PortableServer_ObjectId *objid;

   objid = PortableServer_POA_servant_to_id(servant->poa, servant, ev);
   PortableServer_POA_deactivate_object(servant->poa, objid, ev);
   CORBA_free(objid);

   POA_Xine__fini((PortableServer_Servant) servant, ev);
   g_free(servant);
}

static void
impl_Xine_play(impl_POA_Xine * servant,
	       CORBA_char * MRL,
	       CORBA_unsigned_short position, CORBA_Environment * ev)
{
   xine_play((char *)MRL, position);
}

static void
impl_Xine_pause(impl_POA_Xine * servant, CORBA_Environment * ev)
{
   xine_pause();
}

static void
impl_Xine_stop(impl_POA_Xine * servant, CORBA_Environment * ev)
{
   xine_stop();
}

static CORBA_long
impl_Xine_eject(impl_POA_Xine * servant,
		CORBA_char * MRL, CORBA_Environment * ev)
{
   CORBA_long retval;

   retval = xine_eject(MRL);

   return retval;
}

static CORBA_unsigned_short
impl_Xine__get_position(impl_POA_Xine * servant, CORBA_Environment * ev)
{
   CORBA_unsigned_short retval;

   retval = xine_get_current_position();

   return retval;
}

static Xine_PlayerStatus
impl_Xine__get_status(impl_POA_Xine * servant, CORBA_Environment * ev)
{
   Xine_PlayerStatus retval;

   retval = xine_get_status();

   return retval;
}

static CORBA_unsigned_short
impl_Xine__get_audioChannel(impl_POA_Xine * servant, CORBA_Environment * ev)
{
   CORBA_unsigned_short retval;

   retval = xine_get_audio_channel();

   return retval;
}

static void
impl_Xine__set_audioChannel(impl_POA_Xine * servant,
			    CORBA_unsigned_short value,
			    CORBA_Environment * ev)
{
   xine_select_audio_channel(value);
}

static CORBA_unsigned_short
impl_Xine__get_spuChannel(impl_POA_Xine * servant, CORBA_Environment * ev)
{
   CORBA_unsigned_short retval;

   retval = xine_get_spu_channel();

   return retval;
}

static void
impl_Xine__set_spuChannel(impl_POA_Xine * servant,
			  CORBA_unsigned_short value, CORBA_Environment * ev)
{
   xine_select_spu_channel(value);
}
