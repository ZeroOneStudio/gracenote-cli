/*
 * Copyright (c) 2000-2012 Gracenote.
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
 *  sample client_id client_id_tag license
*/

/*
 *  Modifications made by Ben Bowler <ben@chew.tv>
 *
 *  Output should now be json format and gnid should now be returned
 */

/* GNSDK headers
 *
 * Define the modules your application needs.
 * These constants enable inclusion of headers and symbols in gnsdk.h.
 */
#define GNSDK_MUSICID               1
#define GNSDK_STORAGE_SQLITE		1
#define GNSDK_DSP					1
#include "gnsdk.h"

/* Standard C headers - used by the sample app, but not required for GNSDK */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * Local function declarations
 */
static int
_init_gnsdk(
	const char*				client_id,
	const char*				client_id_tag,
	const char*				client_app_version,
	const char*				license_path,
	gnsdk_user_handle_t*	p_user_handle
	);

static void
_shutdown_gnsdk(
	gnsdk_user_handle_t		user_handle,
	const char*				client_id
	);

static void
_do_sample_musicid_stream(
	gnsdk_user_handle_t		user_handle,
	char*					file
	);

/*
* Sample app start (main)
 */
int
main(int argc, char* argv[])
{
	gnsdk_user_handle_t		user_handle			= GNSDK_NULL;
	const char*				client_id			= NULL;
	const char*				client_id_tag		= NULL;
	const char*				client_app_version	= "1";
	const char*				license_path		= NULL;
	const char*				file_path   		= NULL;
	int						rc					= 0;

	/* Client ID, Client ID Tag and License file must be passed in */

	if (argc == 5)
	{
		client_id = argv[1];
		client_id_tag = argv[2];
		license_path = argv[3];
		file_path = argv[4];

		/* GNSDK initialization */
		rc = _init_gnsdk(
				client_id,
				client_id_tag,
				client_app_version,
				license_path,
				&user_handle
				);
		if (0 == rc)
		{
			/* Perform a sample track TOC query */
			_do_sample_musicid_stream(user_handle, file_path);

			/* Clean up and shutdown */
			_shutdown_gnsdk(user_handle, client_id);
		}
	}
	else
	{
		printf("\nUsage:\n%s clientid clientidtag license file\n", argv[0]);
		rc = -1;
	}

	return rc;
}


/*
* Echo the error and information.
*/
static void
_display_error(
	int				line_num,
	const char*		info,
	gnsdk_error_t	error_code
	)
{
	const	gnsdk_error_info_t*	error_info = gnsdk_manager_error_info();

	/* Error_info will never be GNSDK_NULL.
	 * The SDK will always return a pointer to a populated error info structure.
	 */
	fprintf(stderr,
		"\nerror 0x%08x - %s\n\tline %d, info %s\n",
		error_code,
		error_info->error_description,
		line_num,
		info
		);
}

/*
*    Load existing user handle, or register new one.
*
*    GNSDK requires a user handle instance to perform queries.
*    User handles encapsulate your Gracenote provided Client ID which is unique for your
*    application. User handles are registered once with Gracenote then must be saved by
*    your application and reused on future invocations.
*/
static int
_get_user_handle(
	const char*				client_id,
	const char*				client_id_tag,
	const char*				client_app_version,
	gnsdk_user_handle_t*	p_user_handle
	)
{
	gnsdk_error_t		error				= GNSDK_SUCCESS;
	gnsdk_user_handle_t	user_handle			= GNSDK_NULL;
	char*				user_filename		= NULL;
	size_t				user_filename_len	= 0;
	int					rc					= 0;
	FILE*				file				= NULL;

	user_filename_len = strlen(client_id)+strlen("_user.txt")+1;
	user_filename = malloc(user_filename_len);

	if (NULL != user_filename)
	{
		strcpy(user_filename,client_id);
		strcat(user_filename,"_user.txt");

		/* Do we have a user saved locally? */
		file = fopen(user_filename, "r");
		if (NULL != file)
		{
			gnsdk_char_t serialized_user_string[1024] = {0};

			if (NULL != (fgets(serialized_user_string, 1024, file)))
			{
				/* Create the user handle from the saved user */
				error = gnsdk_manager_user_create(serialized_user_string, &user_handle);
				if (GNSDK_SUCCESS != error)
				{
					_display_error(__LINE__, "gnsdk_manager_user_create()", error);
					rc = -1;
				}
			}
			else
			{
				printf("Error reading user file into buffer.\n");
				rc = -1;
			}
			fclose(file);
		}
		else
		{
			printf("\nInfo: No stored user - this must be the app's first run.\n");
		}

		/* If not, create new one*/
		if (GNSDK_NULL == user_handle)
		{
			error = gnsdk_manager_user_create_new(
						client_id,
						client_id_tag,
						client_app_version,
						&user_handle
						);
			if (GNSDK_SUCCESS != error)
			{
				_display_error(__LINE__, "gnsdk_manager_user_create_new()", error);
				rc = -1;
			}
		}

		free(user_filename);
	}
	else
	{
		printf("Error allocating memory.\n");
		rc = -1;
	}

	if (rc == 0)
	{
		*p_user_handle = user_handle;
	}

	return rc;
}

/*
*    Display product version information.
*/
static void
_display_gnsdk_product_info(void)
{
	/* Display GNSDK Version infomation */
	printf("\nGNSDK Product Version    : v%s \t(built %s)\n", gnsdk_manager_get_product_version(), gnsdk_manager_get_build_date());
}

/*
*  Enable Logging
*/
static int
_enable_logging(void)
{
	gnsdk_error_t	error	= GNSDK_SUCCESS;
	int				rc		= 0;

	error = gnsdk_manager_logging_enable(
				"sample.log",									/* Log file path */
				GNSDK_LOG_PKG_ALL,								/* Include entries for all packages and subsystems */
				GNSDK_LOG_LEVEL_ERROR|GNSDK_LOG_LEVEL_WARNING,	/* Include only error and warning entries */
				GNSDK_LOG_OPTION_ALL,							/* All logging options: timestamps, thread IDs, etc */
				0,												/* Max size of log: 0 means a new log file will be created each run */
				GNSDK_FALSE										/* GNSDK_TRUE = old logs will be renamed and saved */
				);
	if (GNSDK_SUCCESS != error)
	{
		_display_error(__LINE__, "gnsdk_manager_logging_enable()", error);
		rc = -1;
	}

	return rc;
}

/*
* Set the application Locale.
*/
static int
_set_locale(
	gnsdk_user_handle_t		user_handle
	)
{
	gnsdk_locale_handle_t	locale_handle	= GNSDK_NULL;
	gnsdk_error_t			error			= GNSDK_SUCCESS;
	int						rc				= 0;

	error = gnsdk_manager_locale_load(
				GNSDK_LOCALE_GROUP_MUSIC,		/* Locale group */
				GNSDK_LANG_ENGLISH,				/* Languae */
				GNSDK_REGION_DEFAULT,			/* Region */
				GNSDK_DESCRIPTOR_SIMPLIFIED,	/* Descriptor */
				user_handle,					/* User handle */
				GNSDK_NULL,						/* User callback function */
				0,								/* Optional data for user callback function */
				&locale_handle					/* Return handle */
				);
	if (GNSDK_SUCCESS == error)
	{
		/* Setting the 'locale' as default
		 * If default not set, no locale-specific results would be available
		 */
		error = gnsdk_manager_locale_set_group_default(locale_handle);
		if (GNSDK_SUCCESS != error)
		{
			_display_error(__LINE__, "gnsdk_manager_locale_set_group_default()", error);
			rc = -1;
		}

		/* The manager will hold onto the locale when set as default
		 * so it's ok to release our reference to it here
		 */
		gnsdk_manager_locale_release(locale_handle);
	}
	else
	{
		_display_error(__LINE__, "gnsdk_manager_locale_load()", error);
		rc = -1;
	}

	return rc;
}

/*
*     Initializing the GNSDK is required before any other APIs can be called.
*     First step is to always initialize the Manager module, then use the returned
*     handle to initialize any modules to be used by the application.
*
*     For this sample, we also load a locale which is used by GNSDK to provide
*     appropriate locale-sensitive metadata for certain metadata values. Loading of the
*     locale is done here for sample convenience but can be done at anytime in your
*     application.
*/
static int
_init_gnsdk(
	const char*				client_id,
	const char*				client_id_tag,
	const char*				client_app_version,
	const char*				license_path,
	gnsdk_user_handle_t*	p_user_handle
	)
{
	gnsdk_manager_handle_t	sdkmgr_handle	= GNSDK_NULL;
	gnsdk_error_t			error			= GNSDK_SUCCESS;
	gnsdk_user_handle_t		user_handle		= GNSDK_NULL;
	int						rc				= 0;

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
		_display_error(__LINE__, "gnsdk_manager_initialize()", error);
		rc = -1;
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
			_display_error(__LINE__, "gnsdk_storage_sqlite_initialize()", error);
			rc = -1;
		}
	}

	/* Initialize the DSP Library - used for generating fingerprints */
	if (0 == rc)
	{
		error = gnsdk_dsp_initialize(sdkmgr_handle);
		if (GNSDK_SUCCESS != error)
		{
			_display_error(__LINE__, "gnsdk_dsp_initialize()", error);
			rc = -1;
		}
	}

	/* Initialize the MusicID Library */
	if (0 == rc)
	{
		error = gnsdk_musicid_initialize(sdkmgr_handle);
		if (GNSDK_SUCCESS != error)
		{
			_display_error(__LINE__, "gnsdk_musicid_initialize()", error);
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
				&user_handle
				);
	}

	/* Set the 'locale' to return locale-specifc results values. This examples loads an English locale. */
	if (0 == rc)
	{
		rc = _set_locale(user_handle);
	}

	if (0 != rc)
	{
		/* Clean up on failure. */
		_shutdown_gnsdk(user_handle, client_id);
	}
	else
	{
		/* return the User handle for use at query time */
		*p_user_handle = user_handle;
	}

	return rc;
}

/*
*     Call shutdown all initialized GNSDK modules.
*     Release all existing handles before shutting down any of the modules.
*     Shutting down the Manager module should occur last, but the shutdown ordering of
*     all other modules does not matter.
*/
static void
_shutdown_gnsdk(
	gnsdk_user_handle_t		user_handle,
	const char*				client_id
	)
{
	gnsdk_error_t	error							= GNSDK_SUCCESS;
	gnsdk_str_t		updated_serialized_user_string	= GNSDK_NULL;
	char*			user_filename					= GNSDK_NULL;
	size_t			user_filename_len				= 0;
	int				rc								= 0;
	int				return_value					= 0;
	FILE*			file							= NULL;

	/* Release our user handle and see if we need to update our stored version */
	error = gnsdk_manager_user_release(user_handle, &updated_serialized_user_string);
	if (GNSDK_SUCCESS != error)
	{
		_display_error(__LINE__, "gnsdk_manager_user_release()", error);
	}
	else if (GNSDK_NULL != updated_serialized_user_string)
	{
		user_filename_len = strlen(client_id)+strlen("_user.txt")+1;
		user_filename = malloc(user_filename_len);

		if (NULL != user_filename)
		{
			strcpy(user_filename,client_id);
			strcat(user_filename,"_user.txt");

			file = fopen(user_filename, "w");
			if (NULL != file)
			{
				return_value = fputs(updated_serialized_user_string, file);
				if (0 > return_value)
				{
					printf("Error writing user registration file from buffer.\n");
					rc = -1;
				}
				fclose(file);
			}
			else
			{
				printf("\nError: Failed to open the user filename for use in saving the updated serialized user. (%s)\n", user_filename);
			}
			free(user_filename);
		}
		else
		{
			printf("\nError: Failed to allocated user filename for us in saving the updated serialized user.\n");
		}
		gnsdk_manager_string_free(updated_serialized_user_string);
	}

	/* Shutdown the libraries */
	gnsdk_dsp_shutdown();
	gnsdk_musicid_shutdown();
	gnsdk_storage_sqlite_shutdown();
	gnsdk_manager_shutdown();
}

static void
_display_track_gdo(
	gnsdk_gdo_handle_t track_gdo
	)
{
	gnsdk_error_t		error		= GNSDK_SUCCESS;
	// gnsdk_gdo_handle_t	id_gdo      = GNSDK_NULL;
	gnsdk_gdo_handle_t	title_gdo	= GNSDK_NULL;
	gnsdk_gdo_handle_t	album_gdo	= GNSDK_NULL;
	gnsdk_gdo_handle_t	artist_gdo	= GNSDK_NULL;
	// gnsdk_gdo_handle_t	credit_gdo	= GNSDK_NULL;
	gnsdk_cstr_t		value		= GNSDK_NULL;

	// printf("%s\n", track_gdo);
    
    /* Begin json structure */
    printf("{ \n");

   /* track GNID */
	error = gnsdk_manager_gdo_value_get( track_gdo, GNSDK_GDO_VALUE_GNID, 1, &value );
	if (GNSDK_SUCCESS == error)
	{
		printf( " '%s': '%s', \n", "id", value );

		// gnsdk_manager_gdo_release(id_gdo);
	}
	else
	{
		_display_error(__LINE__, "gnsdk_manager_gdo_child_get()", error);
	}
    

	// From docs
	// /gnsdk-3.02.0.422o-20130108/docs/html/index.html#gnsdko_doxygen_html/context_maps.html


	//   Track	 	
 	// GNSDK_GDO_CHILD_ARTISTS	Credited artist(s)
 	// GNSDK_GDO_CHILD_TITLE_OFFICIAL	The Official Title of a Track
 	// GNSDK_GDO_VALUE_GENRE_*	Genre classification of the Track Levels1-3
 	// GNSDK_GDO_VALUE_GNID	GNID. Gracenote identifier - a combination of the TUI and TUI Tag fields.
 	// GNSDK_GDO_VALUE_GNUID	GNUID. Gracenote Identifier
 	// GNSDK_GDO_VALUE_ORDINAL	The ordinal position of this Track in the Album
 	// GNSDK_GDO_VALUE_PRODUCTID	Product identifier
 	// GNSDK_GDO_VALUE_TUI	TUI. Title Unique Identifier. A Gracenote identifier.
 	// GNSDK_GDO_VALUE_TUI_TAG	TUI Tag. A hashed representation of the TUI that guarantees its authenticity.
 	// GNSDK_GDO_VALUE_YEAR	Year originally issued


   /* track year */
	error = gnsdk_manager_gdo_value_get( track_gdo, GNSDK_GDO_VALUE_PRODUCTID, 1, &value );
	if (GNSDK_SUCCESS == error)
	{
		printf( " '%s': '%s', \n", "product_id", value );
		
		// gnsdk_manager_gdo_release(id_gdo);
	}
	else
	{
		_display_error(__LINE__, "gnsdk_manager_gdo_child_get()", error);
	}

	// /* Genres */
 //   /* genre */
	// error = gnsdk_manager_gdo_value_get( track_gdo, GNSDK_GDO_VALUE_GENRE, 1, &value );
	// if (GNSDK_SUCCESS == error)
	// {
	// 	printf( " '%s': '%s', \n", "genre_1", value );
		
	// 	// gnsdk_manager_gdo_release(id_gdo);
	// }
	// else
	// {
	// 	_display_error(__LINE__, "gnsdk_manager_gdo_child_get()", error);
	// }

 //   /* genre */
	// error = gnsdk_manager_gdo_value_get( track_gdo, GNSDK_GDO_VALUE_GENRE_2, 1, &value );
	// if (GNSDK_SUCCESS == error)
	// {
	// 	printf( " '%s': '%s', \n", "genre_2", value );
		
	// 	// gnsdk_manager_gdo_release(id_gdo);
	// }
	// else
	// {
	// 	_display_error(__LINE__, "gnsdk_manager_gdo_child_get()", error);
	// }

 //   /* genre */
	// error = gnsdk_manager_gdo_value_get( track_gdo, GNSDK_GDO_VALUE_GENRE_3, 1, &value );
	// if (GNSDK_SUCCESS == error)
	// {
	// 	printf( " '%s': '%s', \n", "genre_3", value );
		
	// 	// gnsdk_manager_gdo_release(id_gdo);
	// }
	// else
	// {
	// 	_display_error(__LINE__, "gnsdk_manager_gdo_child_get()", error);
	// }

 //   /* track year */
	// error = gnsdk_manager_gdo_value_get( track_gdo, GNSDK_GDO_VALUE_YEAR, 1, &value );
	// if (GNSDK_SUCCESS == error)
	// {
	// 	printf( " '%s': '%s', \n", "year", value );
		
	// 	// gnsdk_manager_gdo_release(id_gdo);
	// }
	// else
	// {
	// 	_display_error(__LINE__, "gnsdk_manager_gdo_child_get()", error);
	// }
    

	/* track Artist */
	error = gnsdk_manager_gdo_child_get( track_gdo, GNSDK_GDO_CHILD_ARTIST, 1, &artist_gdo );
	if (GNSDK_SUCCESS == error)
	{
		error = gnsdk_manager_gdo_child_get( artist_gdo, GNSDK_GDO_CHILD_NAME_OFFICIAL, 1, &title_gdo );
		if (GNSDK_SUCCESS == error)
		{
			error = gnsdk_manager_gdo_value_get( title_gdo, GNSDK_GDO_VALUE_DISPLAY, 1, &value );
			if (GNSDK_SUCCESS == error)
			{
				printf( " '%s': '%s', \n", "artist", value );
			}
			else
			{
				_display_error(__LINE__, "gnsdk_manager_gdo_value_get(GNSDK_GDO_VALUE_DISPLAY artist)", error);
			}
			gnsdk_manager_gdo_release(title_gdo);
		}
		else
		{
			_display_error(__LINE__, "gnsdk_manager_gdo_child_get(GNSDK_GDO_CHILD_TITLE_OFFICIAL artist)", error);
		}
		gnsdk_manager_gdo_release(artist_gdo);
	}
	else
	{
		_display_error(__LINE__, "gnsdk_manager_gdo_child_get(GNSDK_GDO_CHILD_ALBUM track)", error);
	}

	/* track Album */
	error = gnsdk_manager_gdo_child_get( track_gdo, GNSDK_GDO_CHILD_ALBUM, 1, &album_gdo );
	if (GNSDK_SUCCESS == error)
	{
		error = gnsdk_manager_gdo_child_get( album_gdo, GNSDK_GDO_CHILD_TITLE_OFFICIAL, 1, &title_gdo );
		if (GNSDK_SUCCESS == error)
		{
			error = gnsdk_manager_gdo_value_get( title_gdo, GNSDK_GDO_VALUE_DISPLAY, 1, &value );
			if (GNSDK_SUCCESS == error)
			{
				printf( " '%s': '%s', \n", "album", value );
			}
			else
			{
				_display_error(__LINE__, "gnsdk_manager_gdo_value_get(GNSDK_GDO_VALUE_DISPLAY album)", error);
			}
			gnsdk_manager_gdo_release(title_gdo);
		}
		else
		{
			_display_error(__LINE__, "gnsdk_manager_gdo_child_get(GNSDK_GDO_CHILD_TITLE_OFFICIAL album)", error);
		}
		gnsdk_manager_gdo_release(album_gdo);
	}
	else
	{
		_display_error(__LINE__, "gnsdk_manager_gdo_child_get(GNSDK_GDO_CHILD_ALBUM track)", error);
	}

	/* track Title */
	error = gnsdk_manager_gdo_child_get( track_gdo, GNSDK_GDO_CHILD_TITLE_OFFICIAL, 1, &title_gdo );
	if (GNSDK_SUCCESS == error)
	{
		error = gnsdk_manager_gdo_value_get( title_gdo, GNSDK_GDO_VALUE_DISPLAY, 1, &value );
		if (GNSDK_SUCCESS == error)
		{
			printf( " '%s': '%s' \n  }", "title", value ); // Includes end json structure
		}
		else
		{
			_display_error(__LINE__, "gnsdk_manager_gdo_value_get()", error);
		}
		gnsdk_manager_gdo_release(title_gdo);
	}
	else
	{
		_display_error(__LINE__, "gnsdk_manager_gdo_child_get()", error);
	}
    
    
	/* track Label */
	// error = gnsdk_manager_gdo_child_get( track_gdo, GNSDK_GDO_CHILD_CREDIT, 1, &credit_gdo );
	// if (GNSDK_SUCCESS == error)
	// {
	// 	error = gnsdk_manager_gdo_value_get( credit_gdo, GNSDK_GDO_VALUE_DISPLAY, 1, &value );
	// 	if (GNSDK_SUCCESS == error)
	// 	{
	// 		printf( " '%s': '%s', \n", "credit", value );
	// 	}
	// 	else
	// 	{
	// 		_display_error(__LINE__, "gnsdk_manager_gdo_value_get()", error);
	// 	}
	// 	gnsdk_manager_gdo_release(credit_gdo);
	// }
	// else
	// {
	// 	_display_error(__LINE__, "gnsdk_manager_gdo_child_get()", error);
	// }
    
    
    /* End json structure */
    // printf("}");
}

static void
_display_for_resolve(
	gnsdk_gdo_handle_t		response_gdo
	)
{
	gnsdk_error_t			error				= GNSDK_SUCCESS;
	gnsdk_gdo_handle_t		track_gdo			= GNSDK_NULL;
	gnsdk_uint32_t			track_count			= 0;
	gnsdk_uint32_t			track_ordinal		= 0;

	error = gnsdk_manager_gdo_child_count(response_gdo, GNSDK_GDO_CHILD_TRACK, &track_count);
	if (GNSDK_SUCCESS == error)
	{
		printf( "%16s %d\n", "Match count:", track_count);

		/*	Note that the GDO accessors below are *ordinal* based, not index based.  so the 'first' of
			*	anything has a one-based ordinal of '1' - *not* an index of '0'
			*/
		for (track_ordinal = 1; track_ordinal <= track_count; track_ordinal++)
		{
			/* Get the track GDO */
			error = gnsdk_manager_gdo_child_get( response_gdo, GNSDK_GDO_CHILD_TRACK, track_ordinal, &track_gdo );
			if (GNSDK_SUCCESS == error)
			{
				_display_track_gdo(track_gdo);

				/* Release the current track */
				gnsdk_manager_gdo_release(track_gdo);
				track_gdo = GNSDK_NULL;
			}
			else
			{
				_display_error(__LINE__, "gnsdk_manager_gdo_child_get()", error);
			}
		}
	}
	else
	{
		_display_error(__LINE__, "gnsdk_manager_gdo_child_count()", error);
	}
}

static gnsdk_uint32_t
_do_match_selection(gnsdk_gdo_handle_t response_gdo)
{
	/*
	This is where any matches that need resolution/disambiguation are iterated
	and a single selection of the best match is made.

	For this simplified sample, we'll just echo the matches and select the first match.
	*/
	/* _display_for_resolve(response_gdo); */

	return 1;
}

/* This function simulates streaming audio into the Query handle to generate the query fingerprint */
static int
_set_query_fingerprint(
	gnsdk_musicid_query_handle_t	query_handle,
	gnsdk_cstr_t 					file
	)
{
	gnsdk_error_t				error					= GNSDK_SUCCESS;
	gnsdk_bool_t* 				p_blocks_complete		= NULL;
	size_t						read					= 0;
	FILE*						p_file					= NULL;
	char						pcm_audio[2048]			= {0};
	int							rc						= 0;

	 /* check file for existence */
	p_file = fopen(file, "rb");
	if (p_file == NULL)
	{
		printf("\n\n!!!!Failed to open input file: %s!!!\n\n", file);
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
	error = gnsdk_musicid_query_fingerprint_begin(
				query_handle,
				GNSDK_MUSICID_FP_DATA_TYPE_GNFPX,
				44100,
				16,
				2
				);
	if (GNSDK_SUCCESS != error)
	{
		_display_error(__LINE__, "gnsdk_musicidfile_fileinfo_fingerprint_begin()", error);
		fclose(p_file);
		return -1;
	}

	read = fread(pcm_audio, sizeof(char), 2048, p_file);
	while (read > 0)
	{
		 /* write audio to the fingerprinter */
		error = gnsdk_musicid_query_fingerprint_write(
					query_handle,
					pcm_audio,
					read,
					p_blocks_complete /* TODO: if this comes back as true, we have finished */
					);
		if (GNSDK_SUCCESS != error)
		{
			if (GNSDKERR_SEVERE(error)) /* 'aborted' warnings could come back from write which should be expected */
			{
				_display_error(__LINE__, "gnsdk_musicidfile_fileinfo_fingerprint_write()", error);
			}
			rc = -1;
			break;
		}

		read = fread(pcm_audio, sizeof(char), 2048, p_file);
	}

	fclose(p_file);

	 /*signal that we are done*/
	if (GNSDK_SUCCESS == error)
	{
		error = gnsdk_musicid_query_fingerprint_end(query_handle);
		if (GNSDK_SUCCESS != error)
		{
			_display_error(__LINE__, "gnsdk_musicidfile_fileinfo_fingerprint_end()", error);
		}
	}

	return rc;
}

/*
 * This function performs a fingerprint lookup
 */
static void
_do_sample_musicid_stream(
	gnsdk_user_handle_t     user_handle,
	char*					file_path
	)
{
	gnsdk_error_t						error = GNSDK_SUCCESS;
	gnsdk_musicid_query_handle_t		query_handle = GNSDK_NULL;
	gnsdk_gdo_handle_t					response_gdo = GNSDK_NULL;
	gnsdk_gdo_handle_t					track_gdo = GNSDK_NULL;
	gnsdk_gdo_handle_t					followup_response_gdo = GNSDK_NULL;
	gnsdk_uint32_t						count					= 0;
	gnsdk_uint32_t						choice_ordinal			= 0;
	gnsdk_cstr_t						needs_decision			= GNSDK_NULL;
	gnsdk_cstr_t						is_full					= GNSDK_NULL;
	int									rc						= 0;

	/* printf("\n*****Sample MID-Stream Query*****\n"); */

	/* Create the query handle */
	error = gnsdk_musicid_query_create(
				user_handle,
				GNSDK_NULL,	 /* User callback function */
				GNSDK_NULL,	 /* Optional data to be passed to the callback */
				&query_handle
				);

	/* Set the input fingerprint. */
	if (GNSDK_SUCCESS == error)
	{
		rc = _set_query_fingerprint(query_handle, file_path);
		if (0 == rc)
		{
			/* Perform the query */
			error = gnsdk_musicid_query_find_tracks(
						query_handle,
						&response_gdo
						);
			if (GNSDK_SUCCESS != error)
			{
				_display_error(__LINE__, "gnsdk_musicid_query_find_tracks()", error);
			}

			/* See how many tracks were found. */
			if (GNSDK_SUCCESS == error)
			{
				error = gnsdk_manager_gdo_child_count(
								response_gdo,
								GNSDK_GDO_CHILD_TRACK,
								&count
								);
				if (GNSDK_SUCCESS != error)
				{
					_display_error(__LINE__, "gnsdk_manager_gdo_child_count(GNSDK_GDO_CHILD_TRACK)", error);
				}
			}

			/* See if we need any follow-up queries or disambiguation */
			if (GNSDK_SUCCESS == error)
			{
				if (count == 0)
				{
					printf("\nNo tracks found for the input.\n");
				}
				else
				{
					/* we have at least one track, see if disambiguation (match resolution) is necessary. */
					error = gnsdk_manager_gdo_value_get(
								response_gdo,
								GNSDK_GDO_VALUE_RESPONSE_NEEDS_DECISION,
								1,
								&needs_decision
								);
					if (GNSDK_SUCCESS != error)
					{
						_display_error(__LINE__, "gnsdk_manager_gdo_value_get(GNSDK_GDO_VALUE_RESPONSE_NEEDS_DECISION)", error);
					}
					else
					{
						/* See if selection of one of the tracks needs to happen */
						if (0 == strcmp(needs_decision, GNSDK_VALUE_TRUE))
						{
							choice_ordinal = _do_match_selection(response_gdo);
						}
						else
						{
							/* no need for disambiguation, we'll take the first track */
							choice_ordinal = 1;
						}

						error = gnsdk_manager_gdo_child_get(
									response_gdo,
									GNSDK_GDO_CHILD_TRACK,
									choice_ordinal,
									&track_gdo
									);
						if (GNSDK_SUCCESS != error)
						{
							_display_error(__LINE__, "gnsdk_manager_gdo_child_get(GNSDK_GDO_CHILD_TRACK)", error);
						}
						else
						{
							/* See if the track has full data or only partial data. */
							error = gnsdk_manager_gdo_value_get(
										track_gdo,
										GNSDK_GDO_VALUE_FULL_RESULT,
										1,
										&is_full
										);
							if (GNSDK_SUCCESS != error)
							{
								_display_error(__LINE__, "gnsdk_manager_gdo_value_get(GNSDK_GDO_VALUE_FULL_RESULT)", error);
							}
							else
							{
								/* if we only have a partial result, we do a follow-up query to retrieve the full track */
								if (0 == strcmp(is_full, GNSDK_VALUE_FALSE))
								{
									/* do followup query to get full object. Setting the partial track as the query input. */
									error = gnsdk_musicid_query_set_gdo(
												query_handle,
												track_gdo
												);
									if (GNSDK_SUCCESS != error)
									{
										_display_error(__LINE__, "gnsdk_musicid_query_set_gdo()", error);
									}
									else
									{
										/* we can now release the partial track */
										gnsdk_manager_gdo_release(track_gdo);
										track_gdo = GNSDK_NULL;

										error = gnsdk_musicid_query_find_tracks(
													query_handle,
													&followup_response_gdo
													);
										if (GNSDK_SUCCESS != error)
										{
											_display_error(__LINE__, "gnsdk_musicid_query_find_tracks()", error);
										}
										else
										{
											/* now our first track is the desired result with full data */
											error = gnsdk_manager_gdo_child_get(
														followup_response_gdo,
														GNSDK_GDO_CHILD_TRACK,
														1,
														&track_gdo
														);

											/* Release the followup query's response object */
											gnsdk_manager_gdo_release(followup_response_gdo);
										}
									}
								}
							}

							/* We should now have our final, full track result. */
							if (GNSDK_SUCCESS == error)
							{
								// printf( "%16s\n", "Final track:");

								_display_track_gdo(track_gdo);
							}

							gnsdk_manager_gdo_release(track_gdo);
							track_gdo = GNSDK_NULL;
						 }
					}
				}
			}
		}
	}

	/* Clean up */
	/* Release the query handle */
	if (GNSDK_NULL != query_handle)
	{
		gnsdk_musicid_query_release(query_handle);
	}
	/* Release the results */
	if (GNSDK_NULL != response_gdo)
	{
		gnsdk_manager_gdo_release(response_gdo);
	}
}