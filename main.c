/*
 * Copyright (c) 2000 Gracenote.
 *
 * This software may not be used in any way or distributed without
 * permission. All rights reserved.
 *
 * Some code herein may be covered by US and international patents.
 */

/*
 *  Name: musicid_stream
 *  Description:
 *  This example uses MusicID-Stream to fingerprint and identify a music track.
 *
 *  Command-line Syntax:
 *  sample <client_id> <client_id_tag> <license> [local|online] <file>
 */

 /*
 	*  Modifications made by Zero One <we.are@zeroone.st>
 	*
 	*  Add ability to pass file_url as an argument
 	*
 	*  Add _display_track_info_json function that returns track info as JSON
 	*  
 */

/* GNSDK headers
 *
 * Define the modules your application needs.
 * These constants enable inclusion of headers and symbols in gnsdk.h.
 * Define GNSDK_LOOKUP_LOCAL because this program has the potential to do local queries.
 * For local queries, a Gracenote local database must be present.
 */
#define GNSDK_MUSICID_STREAM        1
#define GNSDK_STORAGE_SQLITE        1
#define GNSDK_LOOKUP_LOCAL          1
#define GNSDK_LOOKUP_LOCALSTREAM    1
#define GNSDK_DSP                   1
#include "gnsdk.h"

/* Standard C headers - used by the sample app, but not required for GNSDK */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**********************************************
 *    Local Function Declarations
 **********************************************/
static int
_init_gnsdk(
	const char*          client_id,
	const char*          client_id_tag,
	const char*          client_app_version,
	const char*          license_path,
	int                  use_local,
	gnsdk_user_handle_t* p_user_handle
	);

static void
_shutdown_gnsdk(
	gnsdk_user_handle_t user_handle
	);

static void
_do_sample_musicid_stream(
	gnsdk_user_handle_t user_handle,
	gnsdk_cstr_t s_audio_file
	);

/* callbacks */
gnsdk_void_t GNSDK_CALLBACK_API
_musicidstream_identifying_status_callback(
	gnsdk_void_t* callback_data,
	gnsdk_musicidstream_identifying_status_t status,
	gnsdk_bool_t* pb_abort
	);

gnsdk_void_t GNSDK_CALLBACK_API
_musicidstream_result_available_callback(
	gnsdk_void_t* callback_data,
	gnsdk_musicidstream_channel_handle_t channel_handle,
	gnsdk_gdo_handle_t response_gdo,
	gnsdk_bool_t* pb_abort
	);

gnsdk_void_t GNSDK_CALLBACK_API
_musicidstream_completed_with_error_callback(
	gnsdk_void_t* callback_data,
	gnsdk_musicidstream_channel_handle_t channel_handle,
	const gnsdk_error_info_t* p_error_info
	);

/* Local data */
static gnsdk_cstr_t s_gdb_location = "../../sample_data/sample_db";

/******************************************************************
 *
 *    MAIN
 *
 ******************************************************************/
int
main(int argc, char* argv[])
{
	gnsdk_user_handle_t user_handle        = GNSDK_NULL;
	const char*         client_id          = NULL;
	const char*         client_id_tag      = NULL;
	const char*         client_app_version = "1.0.0.0"; /* Version of your application */
	const char*         license_path       = NULL;
	static gnsdk_cstr_t s_audio_file       = NULL;
	int                 use_local          = -1;
	int                 rc                 = 0;

	if (argc == 6)
	{
		client_id     = argv[1];
		client_id_tag = argv[2];
		license_path  = argv[3];
		s_audio_file  = argv[5];
		if (!strcmp(argv[4], "online"))
		{
			use_local = 0;
		}
		else if (!strcmp(argv[4], "local"))
		{
			use_local = 1;
		}

		/* GNSDK initialization */
		if (use_local != -1)
		{
			rc = _init_gnsdk(
				client_id,
				client_id_tag,
				client_app_version,
				license_path,
				use_local,
				&user_handle
				);
			if (0 == rc)
			{
				/* Perform a sample audio stream query */
				_do_sample_musicid_stream(user_handle, s_audio_file);

				/* Clean up and shutdown */
				_shutdown_gnsdk(user_handle);
			}
		}
	}
	if (argc != 6 || use_local == -1)
	{
		printf("\nUsage:\n%s clientid clientidtag license [local|online]\n", argv[0]);
		rc = -1;
	}

	return rc;

}  /* main() */


/******************************************************************
 *
 *    _DISPLAY_LAST_ERROR
 *
 *    Echo the error and information.
 *
 *****************************************************************/
static void
_display_last_error(
	int line_num
	)
{
	/* Get the last error information from the SDK */
	const gnsdk_error_info_t* error_info = gnsdk_manager_error_info();


	/* Error_info will never be GNSDK_NULL.
	 * The SDK will always return a pointer to a populated error info structure.
	 */
	printf(
		"\nerror from: %s()  [on line %d]\n\t0x%08x %s\n",
		error_info->error_api,
		line_num,
		error_info->error_code,
		error_info->error_description
		);

} /* _display_last_error() */


/******************************************************************
 *
 *    _GET_USER_HANDLE
 *
 *    Load existing user handle, or register new one.
 *
 *    GNSDK requires a user handle instance to perform queries.
 *    User handles encapsulate your Gracenote provided Client ID
 *    which is unique for your application. User handles are
 *    registered once with Gracenote then must be saved by
 *    your application and reused on future invocations.
 *
 *****************************************************************/
static int
_get_user_handle(
	const char*          client_id,
	const char*          client_id_tag,
	const char*          client_app_version,
	int                  use_local,
	gnsdk_user_handle_t* p_user_handle
	)
{
	gnsdk_user_handle_t user_handle               = GNSDK_NULL;
	gnsdk_cstr_t		user_reg_mode             = GNSDK_NULL;
	gnsdk_str_t         serialized_user           = GNSDK_NULL;
	gnsdk_char_t		serialized_user_buf[1024] = {0};
	gnsdk_bool_t        b_localonly               = GNSDK_FALSE;
	gnsdk_error_t       error                     = GNSDK_SUCCESS;
	FILE*               file                      = NULL;
	int                 rc                        = 0;
	
	/* Creating a GnUser is required before performing any queries to Gracenote services,
	 * and such APIs in the SDK require a GnUser to be provided. GnUsers can be created
	 * 'Online' which means they are created by the Gracenote backend and fully vetted.
	 * Or they can be create 'Local Only' which means they are created locally by the
	 * SDK but then can only be used locally by the SDK.
	 */
	
	/* If the application cannot go online at time of user-regstration it should
	 * create a 'local only' user. If connectivity is available, an Online user should
	 * be created. An Online user can do both Local and Online queries.
	 */
	if (use_local)
	{
		user_reg_mode = GNSDK_USER_REGISTER_MODE_LOCALONLY;
	}
	else
	{
		user_reg_mode = GNSDK_USER_REGISTER_MODE_ONLINE;
	}

	/* Do we have a user saved locally? */
	file = fopen("user.txt", "r");
	if (file)
	{
		fgets(serialized_user_buf, sizeof(serialized_user_buf), file);
		fclose(file);
		
		/* Create the user handle from the saved user */
		error = gnsdk_manager_user_create(serialized_user_buf, client_id, &user_handle);
		if (GNSDK_SUCCESS == error)
		{
			error = gnsdk_manager_user_is_localonly(user_handle, &b_localonly);
			if (!b_localonly || (strcmp(user_reg_mode, GNSDK_USER_REGISTER_MODE_LOCALONLY) == 0))
			{
				*p_user_handle = user_handle;
				return 0;
			}
			
			/* else desired regmode is online, but user is localonly - discard and register new online user */
			gnsdk_manager_user_release(user_handle);
		}
		
		if (GNSDK_SUCCESS != error)
		{
			_display_last_error(__LINE__);
		}
	}
	else
	{
		printf("\nInfo: No stored user - this must be the app's first run.\n");
	}

	/*
	 * Register new user
	 */
	error = gnsdk_manager_user_register(
		user_reg_mode,
		client_id,
		client_id_tag,
		client_app_version,
		&serialized_user
	);
	if (GNSDK_SUCCESS == error)
	{
		/* Create the user handle from the newly registered user */
		error = gnsdk_manager_user_create(serialized_user, client_id, &user_handle);
		if (GNSDK_SUCCESS == error)
		{
			/* save newly registered user for use next time */
			file = fopen("user.txt", "w");
			if (file)
			{
				fputs(serialized_user, file);
				fclose(file);
			}
		}
		
		gnsdk_manager_string_free(serialized_user);
	}
	
	if (GNSDK_SUCCESS == error)
	{
		*p_user_handle = user_handle;
		rc = 0;
	}
	else
	{
		_display_last_error(__LINE__);
		rc = -1;
	}

	return rc;

} /* _get_user_handle() */

/******************************************************************
 *
 *    _DISPLAY_EMBEDDED_DB_INFO
 *
 *    Display local Gracenote DB information.
 *
 ******************************************************************/
static void
_display_embedded_db_info(void)
{
	gnsdk_error_t  error       = GNSDK_SUCCESS;
	gnsdk_cstr_t   gdb_version = GNSDK_NULL;
	gnsdk_uint32_t ordinal     = 0;
	
	
	error = gnsdk_lookup_local_storage_info_count(
		GNSDK_LOOKUP_LOCAL_STORAGE_METADATA,
		GNSDK_LOOKUP_LOCAL_STORAGE_GDB_VERSION,
		&ordinal
	);
	if (!error)
	{
		error = gnsdk_lookup_local_storage_info_get(
			GNSDK_LOOKUP_LOCAL_STORAGE_METADATA,
			GNSDK_LOOKUP_LOCAL_STORAGE_GDB_VERSION,
			ordinal,
			&gdb_version
		);
		if (!error)
		{
			printf("Gracenote DB Version : %s\n", gdb_version);
		}
		else
		{
			_display_last_error(__LINE__);
		}
	}
	else
	{
		_display_last_error(__LINE__);
	}
	
}  /* _display_embedded_db_info() */

/******************************************************************
 *
 *    _ENABLE_LOGGING
 *
 *  Enable logging for the SDK. Not used by Sample App. This helps
 *  Gracenote debug your app, if necessary.
 *
 ******************************************************************/
static int
_enable_logging(void)
{
	gnsdk_error_t error = GNSDK_SUCCESS;
	int           rc    = 0;

	error = gnsdk_manager_logging_enable(
		"sample.log",                                           /* Log file path */
		GNSDK_LOG_PKG_ALL,                                      /* Include entries for all packages and subsystems */
		GNSDK_LOG_LEVEL_ERROR,                                  /* Include only error entries */
		GNSDK_LOG_OPTION_ALL,                                   /* All logging options: timestamps, thread IDs, etc */
		0,                                                      /* Max size of log: 0 means a new log file will be created each run */
		GNSDK_FALSE                                             /* GNSDK_TRUE = old logs will be renamed and saved */
		);
	if (GNSDK_SUCCESS != error)
	{
		_display_last_error(__LINE__);
		rc = -1;
	}

	return rc;

}  /* _enable_logging() */


/*****************************************************************************
 *
 *    _SET_LOCALE
 *
 *  Set application locale. Note that this is only necessary if you are using
 *  locale-dependant fields such as genre, mood, origin, era, etc. Your app
 *  may or may not be accessing locale_dependent fields, but it does not hurt
 *  to do this initialization as a matter of course .
 *
 ****************************************************************************/
static int
_set_locale(
	gnsdk_user_handle_t user_handle
	)
{
	gnsdk_locale_handle_t locale_handle = GNSDK_NULL;
	gnsdk_error_t         error         = GNSDK_SUCCESS;
	int                   rc            = 0;


	error = gnsdk_manager_locale_load(
		GNSDK_LOCALE_GROUP_MUSIC,               /* Locale group */
		GNSDK_LANG_ENGLISH,                     /* Language */
		GNSDK_REGION_DEFAULT,                   /* Region */
		GNSDK_DESCRIPTOR_SIMPLIFIED,            /* Descriptor */
		user_handle,                            /* User handle */
		GNSDK_NULL,                             /* User callback function */
		0,                                      /* Optional data for user callback function */
		&locale_handle                          /* Return handle */
		);
	if (GNSDK_SUCCESS == error)
	{
		/* Setting the 'locale' as default
		 * If default not set, no locale-specific results would be available
		 */
		error = gnsdk_manager_locale_set_group_default(locale_handle);
		if (GNSDK_SUCCESS != error)
		{
			_display_last_error(__LINE__);
			rc = -1;
		}

		/* The manager will hold onto the locale when set as default
		 * so it's ok to release our reference to it here
		 */
		gnsdk_manager_locale_release(locale_handle);
	}
	else
	{
		_display_last_error(__LINE__);
		rc = -1;
	}

	return rc;

}  /* _set_locale() */


/****************************************************************************************
 *
 *    _INIT_GNSDK
 *
 *     Initializing the GNSDK is required before any other APIs can be called.
 *     First step is to always initialize the Manager module, then use the returned
 *     handle to initialize any modules to be used by the application.
 *
 *     For this sample, we also load a locale which is used by GNSDK to provide
 *     appropriate locale-sensitive metadata for certain metadata values. Loading of the
 *     locale is done here for sample convenience but can be done at anytime in your
 *     application.
 *
 ****************************************************************************************/
static int
_init_gnsdk(
	const char*          client_id,
	const char*          client_id_tag,
	const char*          client_app_version,
	const char*          license_path,
	int                  use_local,
	gnsdk_user_handle_t* p_user_handle
	)
{
	gnsdk_manager_handle_t sdkmgr_handle = GNSDK_NULL;
	gnsdk_error_t          error         = GNSDK_SUCCESS;
	gnsdk_user_handle_t    user_handle   = GNSDK_NULL;
	int                    rc            = 0;


	/* Display GNSDK Product Version Info */
	/* _display_gnsdk_product_info(); */

	/* Initialize the GNSDK Manager */
	error = gnsdk_manager_initialize(
		&sdkmgr_handle,
		license_path,
		GNSDK_MANAGER_LICENSEDATA_FILENAME
		);
	if (GNSDK_SUCCESS != error)
	{
		_display_last_error(__LINE__);
		return -1;
	}

	/* Enable logging */
	if (0 == rc)
	{
		rc = _enable_logging();
	}

	/* Initialize the Storage SQLite Library */
	if (0 == rc)
	{
		error = gnsdk_storage_sqlite_initialize(sdkmgr_handle);
		if (GNSDK_SUCCESS != error)
		{
			_display_last_error(__LINE__);
			rc = -1;
		}
	}
	
	if (use_local)
	{
		/* Set folder location of local database */
		if (0 == rc)
		{
			error = gnsdk_storage_sqlite_option_set(
				GNSDK_STORAGE_SQLITE_OPTION_STORAGE_FOLDER,
				s_gdb_location
			);
			if (GNSDK_SUCCESS != error)
			{
				_display_last_error(__LINE__);
				rc = -1;
			}
		}
		
		/* Initialize the Lookup Local Library */
		if (0 == rc)
		{
			error = gnsdk_lookup_local_initialize(sdkmgr_handle);
			if (GNSDK_SUCCESS != error)
			{
				_display_last_error(__LINE__);
				rc = -1;
			}
			else
			{
				/* Display information about our local EDB */
				_display_embedded_db_info();
			}
		}

		/* Initialize the Lookup LocalStream Library */
		if (0 == rc)
		{
			error = gnsdk_lookup_localstream_initialize(sdkmgr_handle);
			if (GNSDK_SUCCESS != error)
			{
				_display_last_error(__LINE__);
				rc = -1;
			}
		}
		
		if (0 == rc)
		{
			error = gnsdk_lookup_localstream_storage_location_set(s_gdb_location);
			if (GNSDK_SUCCESS != error)
			{
				_display_last_error(__LINE__);
				rc = -1;
			}
		}
	}

	/* Initialize the DSP Library - used for generating fingerprints */
	if (0 == rc)
	{
		error = gnsdk_dsp_initialize(sdkmgr_handle);
		if (GNSDK_SUCCESS != error)
		{
			_display_last_error(__LINE__);
			rc = -1;
		}
	}

	/* Initialize the MusicID-Stream Library */
	if (0 == rc)
	{
		error = gnsdk_musicidstream_initialize(sdkmgr_handle);
		if (GNSDK_SUCCESS != error)
		{
			_display_last_error(__LINE__);
			rc = -1;
		}
	}
	
	/* Get a user handle for our client ID.  This will be passed in for all queries */
	if (0 == rc)
	{
		rc = _get_user_handle(
			client_id,
			client_id_tag,
			client_app_version,
			use_local,
			&user_handle
			);
	}
	
	/* Set the user option to use our local Gracenote DB unless overridden. */
	if (use_local)
	{
		if (0 == rc)
		{
			error = gnsdk_manager_user_option_set(
				user_handle,
				GNSDK_USER_OPTION_LOOKUP_MODE,
				GNSDK_LOOKUP_MODE_LOCAL
			);
			if (GNSDK_SUCCESS != error)
			{
				_display_last_error(__LINE__);
				rc = -1;
			}
		}
	}

	/* Set the 'locale' to return locale-specifc results values. This examples loads an English locale. */
	if (0 == rc)
	{
		rc = _set_locale(user_handle);
	}

	if (0 != rc)
	{
		/* Clean up on failure. */
		_shutdown_gnsdk(user_handle);
	}
	else
	{
		/* return the User handle for use at query time */
		*p_user_handle = user_handle;
	}

	return rc;

}  /* _init_gnsdk() */


/***************************************************************************
 *
 *    _SHUTDOWN_GNSDK
 *
 *     When your program is terminating, or you no longer need GNSDK, you should
 *     call gnsdk_manager_shutdown(). No other shutdown operations are required.
 *     gnsdk_manager_shutdown() also shuts down all other modules, regardless
 *     of the number of times they have been initialized.
 *     You can shut down individual modules while your program is running with
 *     their dedicated shutdown functions in order to free up resources.
 *
 ***************************************************************************/
static void
_shutdown_gnsdk(
	gnsdk_user_handle_t user_handle
	)
{
	gnsdk_error_t error = GNSDK_SUCCESS;

	error = gnsdk_manager_user_release(user_handle);
	if (GNSDK_SUCCESS != error)
	{
		_display_last_error(__LINE__);
	}

	/* Shutdown the Manager to shutdown all libraries */
	gnsdk_manager_shutdown();

}  /* _shutdown_gnsdk() */


/***************************************************************************
 *
 *    _display_track_info_json
 *
 ***************************************************************************/

static void
_display_track_info_json(
	gnsdk_gdo_handle_t track_gdo
	)
{
	gnsdk_error_t		error		= GNSDK_SUCCESS;
	gnsdk_gdo_handle_t	title_gdo	= GNSDK_NULL;
	gnsdk_gdo_handle_t	artist_gdo	= GNSDK_NULL;
	gnsdk_cstr_t		value		= GNSDK_NULL;

  /* Begin json structure */
  printf("{");

	/* Artist Title */
	error = gnsdk_manager_gdo_child_get( track_gdo, GNSDK_GDO_CHILD_ARTIST, 1, &artist_gdo );
	if (GNSDK_SUCCESS == error)
	{
		error = gnsdk_manager_gdo_child_get( artist_gdo, GNSDK_GDO_CHILD_NAME_OFFICIAL, 1, &title_gdo );
		if (GNSDK_SUCCESS == error)
		{
			error = gnsdk_manager_gdo_value_get( title_gdo, GNSDK_GDO_VALUE_DISPLAY, 1, &value );
			if (GNSDK_SUCCESS == error)
			{
				printf( "\"%s\": \"%s\", ", "artist", value );
			}
			else
			{
				_display_last_error(__LINE__);
			}
			gnsdk_manager_gdo_release(title_gdo);
		}
		else
		{
			_display_last_error(__LINE__);
		}
		gnsdk_manager_gdo_release(artist_gdo);
	}
	else
	{
		_display_last_error(__LINE__);
	}

	/* Album Title */
	error = gnsdk_manager_gdo_child_get( track_gdo, GNSDK_GDO_CHILD_TITLE_OFFICIAL, 1, &title_gdo );
	if (GNSDK_SUCCESS == error)
	{
		error = gnsdk_manager_gdo_value_get( title_gdo, GNSDK_GDO_VALUE_DISPLAY, 1, &value );
		if (GNSDK_SUCCESS == error)
		{
			printf( "\"%s\": \"%s\", ", "album", value );
		}
		else
		{
			_display_last_error(__LINE__);
		}
		gnsdk_manager_gdo_release(title_gdo);
	}
	else
	{
		_display_last_error(__LINE__);
	}

	/* Track Title */
	error = gnsdk_manager_gdo_child_get( track_gdo, GNSDK_GDO_CHILD_TITLE_OFFICIAL, 1, &title_gdo );
	if (GNSDK_SUCCESS == error)
	{
		error = gnsdk_manager_gdo_value_get( title_gdo, GNSDK_GDO_VALUE_DISPLAY, 1, &value );
		if (GNSDK_SUCCESS == error)
		{
			printf( "\"%s\": \"%s\"", "title", value );
		}
		else
		{
			_display_last_error(__LINE__);
		}
		gnsdk_manager_gdo_release(title_gdo);
	}
	else
	{
		_display_last_error(__LINE__);
	}
    
  printf("}");    
}

/***************************************************************************
 *
 *    _PROCESS_AUDIO
 *
 * This function simulates streaming audio into the Channel handle to give
 * MusicId-Stream audio to identify
 *
 ***************************************************************************/
static int
_process_audio(
	gnsdk_musicidstream_channel_handle_t channel_handle,
	gnsdk_cstr_t s_audio_file
	)
{
	gnsdk_error_t error           = GNSDK_SUCCESS;
	gnsdk_size_t  read_size       = 0;
	gnsdk_byte_t  pcm_audio[2048] = {0};
	FILE*         p_file          = NULL;
	int           rc              = 0;

	/* check file for existence */
	p_file = fopen(s_audio_file, "rb");
	if (p_file == NULL)
	{
		printf("\n\n!!!!Failed to open input file: %s!!!\n\n", s_audio_file);
		return -1;
	}

	/* skip the wave header (first 44 bytes). we know the format of our sample files */
	if (0 != fseek(p_file, 44, SEEK_SET))
	{
		fclose(p_file);
		return -1;
	}

	/* initialize the fingerprinter
	   Note: The sample file shipped is a 44100Hz 16-bit stereo (2 channel) wav file */
	error = gnsdk_musicidstream_channel_audio_begin(
		channel_handle,
		44100, 16, 2
		);
	if (GNSDK_SUCCESS != error)
	{
		_display_last_error(__LINE__);
		fclose(p_file);
		return -1;
	}

	/* To keep this sample single-threaded, we launch the identification request
	 ** immediately then do the audio processing. Generally we expect this
	 ** call to occur on a separate thread from audio processing thread.
	 **
	 ** MusicId-Stream will actually perform the identification when it
	 ** receives enough audio.
	 **
	 ** With the asynchronous nature of MusicID-Stream this call is non-blocking so it is ok to
	 ** call on the UI thread.
	 */
	error = gnsdk_musicidstream_channel_identify(channel_handle);
	if (GNSDK_SUCCESS != error)
	{
		_display_last_error(__LINE__);
		fclose(p_file);
		return -1;
	}

	read_size = fread(pcm_audio, sizeof(char), 2048, p_file);
	while (read_size > 0)
	{
		/* write audio to the fingerprinter */
		error = gnsdk_musicidstream_channel_audio_write(
			channel_handle,
			pcm_audio,
			read_size
			);
		if (GNSDK_SUCCESS != error)
		{
			if (GNSDKERR_SEVERE(error)) /* 'aborted' warnings could come back from write which should be expected */
			{
				_display_last_error(__LINE__);
			}
			rc = -1;
			break;
		}

		read_size = fread(pcm_audio, sizeof(char), 2048, p_file);
	}

	fclose(p_file);

	/*signal that we are done*/
	if (GNSDK_SUCCESS == error)
	{
		error = gnsdk_musicidstream_channel_audio_end(channel_handle);
		if (GNSDK_SUCCESS != error)
		{
			_display_last_error(__LINE__);
		}
	}

	return rc;

}  /* _process_audio() */


/***************************************************************************
 *
 *    _DO_SAMPLE_MUSICID_STREAM
 *
 ***************************************************************************/
static void
_do_sample_musicid_stream(
	gnsdk_user_handle_t user_handle,
	gnsdk_cstr_t s_audio_file
	)
{
	gnsdk_musicidstream_channel_handle_t channel_handle = GNSDK_NULL;
	gnsdk_musicidstream_callbacks_t      callbacks      = {0};
	gnsdk_error_t                        error          = GNSDK_SUCCESS;
	int                                  rc             = 0;

	/* printf("\n*****Sample MID-Stream Query*****\n"); */

	/* MusicId-Stream requires callbacks to receive identification results.
	** He we set the various callbacks for results ands status.
	*/
	callbacks.callback_status             = GNSDK_NULL; /* not used in this sample */
	callbacks.callback_processing_status  = GNSDK_NULL; /* not used in this sample */
	callbacks.callback_identifying_status = _musicidstream_identifying_status_callback;
	callbacks.callback_result_available   = _musicidstream_result_available_callback;
	callbacks.callback_error              = _musicidstream_completed_with_error_callback;

	/* Create the channel handle */
	error = gnsdk_musicidstream_channel_create(
		user_handle,
		gnsdk_musicidstream_preset_radio,
		&callbacks,          /* User callback functions */
		GNSDK_NULL,          /* Optional data to be passed to the callbacks */
		&channel_handle
	);
	if (GNSDK_SUCCESS == error)
	{
		rc = _process_audio(channel_handle, s_audio_file);
		if (0 == rc)
		{
			/* result will be sent to _musicidstream_result_available_callback */
		}

		/* wait for the identification to finish so we actually get results */
		gnsdk_musicidstream_channel_wait_for_identify(channel_handle, GNSDK_MUSICIDSTREAM_TIMEOUT_INFINITE);
	}

	/* Clean up */
	gnsdk_musicidstream_channel_release(channel_handle);

}   /* _do_sample_musicid_stream() */


/*-----------------------------------------------------------------------------
 *  _musicidstream_identifying_status_callback
 */
gnsdk_void_t GNSDK_CALLBACK_API
_musicidstream_identifying_status_callback(
	gnsdk_void_t*                            callback_data,
	gnsdk_musicidstream_identifying_status_t status,
	gnsdk_bool_t*                            pb_abort
	)
{
	/* printf("\n%s: status = %d\n\n", __FUNCTION__, status); */
	/* This sample chooses to stop the audio processing when the identification
	** is complete so it stops feeding in audio */
	if (status == gnsdk_musicidstream_identifying_ended)
	{
		/* printf("\n%s aborting\n\n", __FUNCTION__); */
		*pb_abort = GNSDK_TRUE;
	}

	GNSDK_UNUSED(callback_data);
}


/*-----------------------------------------------------------------------------
 *  _musicidstream_result_available_callback
 */
gnsdk_void_t GNSDK_CALLBACK_API
_musicidstream_result_available_callback(
	gnsdk_void_t*                        callback_data,
	gnsdk_musicidstream_channel_handle_t channel_handle,
	gnsdk_gdo_handle_t                   response_gdo,
	gnsdk_bool_t*                        pb_abort
	)
{
	gnsdk_gdo_handle_t track_gdo = GNSDK_NULL;
	gnsdk_uint32_t     count     = 0;
	gnsdk_error_t      error     = GNSDK_SUCCESS;

	/* See how many albums were found. */
	error = gnsdk_manager_gdo_child_count(
		response_gdo,
		GNSDK_GDO_CHILD_ALBUM,
		&count
		);
	if (GNSDK_SUCCESS != error)
	{
		_display_last_error(__LINE__);
	}
	else
	{
		if (count != 0)
		{
			/* we display first album result */
			error = gnsdk_manager_gdo_child_get(
				response_gdo,
				GNSDK_GDO_CHILD_ALBUM,
				1,
				&track_gdo
				);
			if (GNSDK_SUCCESS != error)
			{
				_display_last_error(__LINE__);
			}
			else
			{
				_display_track_info_json(track_gdo);
				gnsdk_manager_gdo_release(track_gdo);
			}
		}
	}

	GNSDK_UNUSED(pb_abort);
	GNSDK_UNUSED(callback_data);
	GNSDK_UNUSED(channel_handle);
}


/*-----------------------------------------------------------------------------
 *  _musicidstream_completed_with_error_callback
 */
gnsdk_void_t GNSDK_CALLBACK_API
_musicidstream_completed_with_error_callback(
	gnsdk_void_t*                        callback_data,
	gnsdk_musicidstream_channel_handle_t channel_handle,
	const gnsdk_error_info_t*            p_error_info
	)
{
	/* an error occurred during identification */
	printf(
		"\nerror from: %s()  [error callback]\n\t0x%08x %s",
		p_error_info->error_api,
		p_error_info->error_code,
		p_error_info->error_description
		);

	GNSDK_UNUSED(channel_handle);
	GNSDK_UNUSED(callback_data);
}