{
  'variables': {
    'variables': {
      # Turns on for low-end specific optimizations and vice-versa.
      'low_end_target%': 0,

      # Disable components only used by Samsung's Android browser (and breaks building TestShell)
      'enable_s_android_browser%': 0,
	  
	  # Disable components only used by Samsung's Android browser (and breaks building TestShell)
      'enable_kerberos_feature%': 1,

      # Disable components only used by Samsung's Android browser (and breaks building UnitTest)
      'enable_s_android_browser_unittest%': 0,

      #flag for enabling the sbrowser trunk build with gcc compiler 4.8
      'enable_ndktoolchain_48%': 1,

      # Disable support for building desktop chrome by default
      'enable_desktop_chrome%': 0,

      # Disable trace events by default. Enable them if you need to use about:tracing
      'enable_trace_events%': 0,

      # Add JS API on the Window object to set default search engine
      'enable_setting_search_from_js%': 1,

      # Turns on for Qualcomm device.
      'enable_qualcomm_v8%': 0,

      # Turns on for SLSI device.
      'enable_samsung_v8%': 0,

      # V8 product name.
      'v8_product%': '',

      #HTML5 FLAGS
      'canvas_path%': 1,#if this flag is made 0 please comment the corresponding flag "S_OS_CANVAS_PATH" in CanvasRenderingContext2D.idl

      # Remove chome specific features
      'enable_remove_chrome_specific_features%': 0,
      'disable_breakpad%': 0,
      'disable_perf_monitor%': 0,
      'enable_extensions_all%': 1,
      'enable_webui%': 1,
      'enable_signin%': 1,
      'enable_sync%': 1,

      # Push API feature
      'enable_push_api_features%' : 0,
    },
    'android_libs_dir%': '<!(cd <(DEPTH) && pwd -P)/platform/kk/libs',
    'android_src%': '<!(cd <(DEPTH) && pwd -P)/platform/kk',	
    'low_end_target%': '<(low_end_target)',
    'enable_s_android_browser%': '<(enable_s_android_browser)',
    'enable_s_android_browser_unittest%': '<(enable_s_android_browser_unittest)',
    'enable_ndktoolchain_48%': '<(enable_ndktoolchain_48)',
    'enable_desktop_chrome%': '<(enable_desktop_chrome)',
    'enable_trace_events%': '<(enable_trace_events)',
    'enable_setting_search_from_js%': '<(enable_setting_search_from_js)',
    'enable_qualcomm_v8%': '<(enable_qualcomm_v8)',
    'enable_samsung_v8%': '<(enable_samsung_v8)',
    'v8_product%': '<(v8_product)',
    'enable_remove_chrome_specific_features%': '<(enable_remove_chrome_specific_features)',
    'disable_breakpad%': '<(disable_breakpad)',
    'canvas_path%': '<(canvas_path)',
    'disable_perf_monitor%': '<(disable_perf_monitor)',
    'android_wbs_lib_dir%': '<!(cd <(DEPTH) && pwd -P)/platform/kk/libs',
    'android_wbs_src%': '<!(cd <(DEPTH) && pwd -P)/platform/kk',
    'enable_push_api_features%': '<(enable_push_api_features)',
    'enable_extensions_all%': '<(enable_extensions_all)',
    'enable_webui%': '<(enable_webui)',
    'enable_signin%': '<(enable_signin)',
    'enable_sync%': '<(enable_sync)',

    # Configure library size optimizations by linking agaist system libraries or disabling any feature listed below.
    'use_system_libxml': 0,
    'use_system_icu': 0,
    'use_system_zlib': 0,
    'enable_webrtc': 0,

    # Disable debug logs and some other developer-only features
    'buildtype': 'Official',

    # Re-enable using mmap() to speed up the disk cache for ARM Android - see net/net.gyp
    'posix_avoid_mmap': 0,

    # Enable neon at compile time instead of runtime.
    'arm_neon': 1,
    'arm_neon_optional': 0,

    'conditions': [
      ['OS=="android" and low_end_target!=1', {
        # Enable the Android Chromium linker to share ELF .data.rel.ro segments between renderers and save RAM.
        # FIXME(cdumez): This is disabled on low end for now as it does not work on HEAT device.
        # Chromium linker is disabled on both high-end and low-end because it causes crash on SBrowser testshell
        'use_chromium_linker': 0,
        # webrtc is available only in a high-end-device.
        'enable_webrtc': 1,
      }],
      ['OS=="android" and low_end_target==1', {
       ## Feature Reduction
        'enable_autofill': 0,
        'enable_mostvisited': 0,
        'enable_autofill_dialog': 0,
        'autofill_enable_sync': 0,
        'enable_printing': 0,
        'enable_webui_omnibox': 0,
        'disable_ftp_support%': 1,
        'disable_webgl': 1,
        'disable_autocomplete': 1,
        'disable_spdy_quic': 1,
        'disable_angle': 1, #should not be disabled when webgl is enabled because of glsl translation/validation for webgl
        'disable_libphonenumber': 1,
        'disable_libwebp': 1,
        'enable_extensions_all': 0, #must be used together with enable_extensions. This flag removes all extensions reference
        'enable_webui': 0,
        'enable_signin': 0,
        'enable_sync': 1,
        'enable_file_system_sync':0,
        'enable_managed_users': 0,
        'enable_crashes': 0,
    	'proprietary_codecs%': 0,
    	'use_openmax_dl_fft%': 0,
# NOT REQUIRED FOR SBROWSER (these are google chrome self features)
        'enable_extensions':0,
        'enable_automation':0,
        'enable_google_now':0,
        'enable_task_manager':0,
        'enable_prerender': 0,
        'enable_remove_chrome_specific_features':0,
        'disable_breakpad':1,
        'disable_perf_monitor':1,
        'v8_enable_debugger_support':0,
        'enable_devtools': 0,
        'enable_translate': 0,

        ##Enable low end patches forcibly
        'enable_low_end_feature': 1,
        'enable_4444_texture': 1,

       ##Static DATA reduction
        'v8_use_snapshot' : 'false',

       ##System library reuse
        'use_system_icu': 1,
        'use_system_libjpeg': 1,
        'use_system_libpng': 1,
        'use_system_expat': 1,
        'use_system_harfbuzz_ng': 1,
        'use_system_stlport':1,
        # Disable temporarily because it causes crash under android 4.4.4
        #'use_system_libxml': 1,
        'use_system_zlib': 1,
        'use_system_freetype': 1,
        'use_system_sqlite': 1,
        # Disable temporarily because it causes crash under android 4.4.4
        #'use_system_skia': 1,
        # Build env for system library reuse
        # NOTE!!!
        #    for now, SBrowser 2.0 official build system is not built under platform.
        #    So we have to copy platform libs/code inside sbrowser
        'android_libs_dir%': '<!(cd <(DEPTH) && pwd -P)/platform/kk/libs',
        'android_src%': '<!(cd <(DEPTH) && pwd -P)/platform/kk',
      }],
      ['enable_desktop_chrome!=1', {
        # Match the Android configuration on Linux by default
        'disable_nacl': 1,
        'enable_automation': 0,
        'enable_background': 0,
        'enable_extensions': 0,
        'enable_google_now': 0,
        'enable_pepper_cdms': 0,
        'enable_plugins': 0,
        'enable_spellcheck': 0,
        'remoting': 0,
        'use_cups': 0,
        #'use_kerberos': 0,

        #FIXME bug(137) - adjust test expecations for tests/media/media-source
        # Enable propriatery codecs
        #'proprietary_codecs': 1,

        #FIXME - bug(138) - LayoutTests on Aura on Linux are not yet stable
        # To disable toolkit_uses_gtk (to match Android) we need to enable aura
        #'toolkit_uses_gtk': 0,
        #'use_aura': 1,
      }],
      ['enable_ndktoolchain_48==1', {
        'gcc_version%': 48,
      }],
      ['enable_s_android_browser==1', {
        'enable_extensions%': 1,
       ##Disable chrome browser features not used by SBrowser in both low and high end devices
        'enable_translate': 0,
        'enable_file_system_sync':0,
        'enable_managed_users': 0,
        'enable_crashes': 0,
        'enable_signin': 0,
        'disable_perf_monitor':1,
        'enable_extensions_all%': 1,
       #Enabled the Notification system for Android.
        'notifications%': 1,
        'grit_defines': [
          '-D', 'enable_notifications',
          '-D', 'enable_extensions',
	  '-D', 'enable_s_android_browser',
        ],
      }],
      ['enable_qualcomm_v8==1', {
        'v8_product': 'ssecv8',
      }, {
        'v8_product': 'secv8',
        'enable_samsung_v8%': 1,
      }],
      ['enable_s_android_browser_unittest==1', {
        'enable_qualcomm_v8%': 0,
        'enable_samsung_v8%': 0,
      }],
    ],
  },
  'target_defaults': {
    'conditions': [
      ['enable_remove_chrome_specific_features==1', {
        'defines': ['S_REMOVE_CHROME_SPECIFIC_FEATURES=1'],
      }],
      ['enable_webrtc==1', {
        'defines': [
        'ENABLE_WEBRTC_H264_CODEC=1',
        'ENABLE_WEBRTC_HW_ACCEL=1',
        ],
      }],
      ['disable_breakpad==1', {
        'defines': ['S_DISABLE_BREAKPAD=1'],
      }],
      ['canvas_path==1', {
        'defines': ['S_OS_CANVAS_PATH=1'],#if this flag is made 0 please comment the corresponding flag "S_OS_CANVAS_PATH" in CanvasRenderingContext2D.idl
      }],
      ['disable_perf_monitor==1', {
        'defines': ['S_DISABLE_PERF_MONITOR=1'],
      }],
      ['enable_extensions_all==1', {
        'defines': ['ENABLE_EXTENSIONS_ALL=1'],
      }],
      ['enable_webui==1', {
        'defines': ['ENABLE_WEBUI=1'],
      }],
      ['enable_signin==1', {
        'defines': ['ENABLE_SIGNIN=1'],
      }],
      ['enable_sync==1', {
        'defines': ['ENABLE_SYNC=1'],
      }],
      ['enable_trace_events==1', {
        'defines': ['ENABLE_TRACE_EVENTS=1'],
      }],
      ['enable_setting_search_from_js==1', {
        'defines': ['ENABLE_BING_SEARCH_ENGINE_SETTING_FROM_JS=1'],
      }],
      ['enable_push_api_features==1', {
        'defines': ['ENABLE_PUSH_API=1'],
      }],
      ['enable_s_android_browser==1', {
        'defines': [
            'S_NATIVE_SUPPORT=1',
            #Feature Flags
            'ENABLE_FIT_TO_MAJOR=1',
            'S_ACCELERATED_ROTATE_TRANSFORM=1',
            'S_NOTIFY_ROTATE_STATUS=1',
            'SBROWSER_PRINT_PAINT_LOG=1',
	        'SBROWSER_OVERVIEW_MODE=1',
            'S_SCROLL_EVENT=1',
            #'S_IME_SCROLL_EVENT=1',
            'S_SAVE_PAGE_AS_HTML=1',   # Adding for support of Save page as html for Annotation feature
            'S_CONTEXT_MENU_FLAG=1',
            'S_FORM_NAVIGATION_SCROLL=1',
            'SBROWSER_GRAPHICS_GETBITMAP=1',
            'SBROWSER_MAX_RASTER_BYTES_MODIFICATION=1',
            'SBROWSER_PASSWORD_ENCRYPTION=1', #Flag for password encryption
            #'SBROWSER_ENCRYPTION_KEY_CHANGE=1', #Flag for encryption key change
            'SBROWSER_CSC_FEATURE=1', 
            'SBROWSER_DEFERS_LOADING=1', 
            'SBROWSER_DEFAULT_MAX_FRAMES_PENDING=1', # Flag used to increase a pendding frame capability
            'SBROWSER_ENABLE_JPN_COMPOSING_REGION=1', # Flag for sub composing region using JPN IME
            #Bug Fixes
            'SBROWSER_CURSOR_BLINKING=1', #To enable cursor blinking on longpress
	     'S_PLM_P140427_00435=1', #Crash in ArchiveResourceCollection during monkey test
	     'S_PLM_P140501_03118=1', #crash in simple font data during monkey test
            'S_TEXT_SELECTION_BUGFIXES=1', # Text selection Bug Fixes
            'S_TEXT_SELECTION_MODIFIEDBOUNDS=1', # SBrowser Text selection Bounds calculation is different from Chrome.
	     'S_PLM_P140430_04580=1',#Opensource issue Fragment navigations not happening proper in wml pages
	     'S_OPEN_SOURCE_266793003_PATCH=1',#Avoid Remember Password Popup when user perform action outside page(eg: typed url, back press, history navigation)
             'S_PLM_P140430_03129=1', #To enable alt text for image
	     'S_FOCUS_RING_FIX=1', #To Avoid highlighting while scrolling
             'S_PLM_P140430_02807=1', #Auto detect is not set on renderer initiation which is not reflecting proper text encoding
             'S_PLM_P140430_01162=1',  #In find on page can not defferentiate selected and found text color
             'S_PLM_P130710_4354=1',  # to fix find on page zoom issue
             'S_PLM_P130702_1591=1', #context menu issue fix
             'S_PLM_P140510_01709=1',#Merging latest wtf partition changes improve memory allocation and to reduce OOM scenarios
	     'S_PLM_P140519_04934=1',#Black Patch is displayed at bottom of thumbnail in tabmngr from mostvisited page
	     'S_PLM_P140507_05160=1',#Support for detecting webgl or canvas2d content within viewport and returning without capturing softbitmap in this scenario
	     'S_PLM_P140509_06237=1', #fix for save page embedded video	
	     'S_BLOCK_AUTOFILL_IN_SEARCH_FIELD=1',#Fix to block updating text change in search field
	     'S_PLM_P140529_01584=1',#Fix for once crash on ScrollBartheme Dev:Muscat EUR OPEN S/W ver NE2
#	     'S_PLM_P140529_05383=1',  #Preventive null check for crash
	     'S_PLM_P140604_05276=1', #Random crash on saving page
	     'S_PLM_P140609_04086=1', #Preventive null check to avoid crash
#	     'S_PLM_P140607_01108=1', # Progress bar blinking fix
	     'S_PLM_P140605_06393=1', # Background color of selection text is different
	     'S_PLM_P140617_01760=1', # Preventive null check to avoid crash
	     'S_PLM_P140616_04291=1', # BT keyboard long press fix
     	     'S_PLM_P140607_02212=1', #  FIX for full background doesn't become Dark on clicking to checkbox on google translate feedback page.
     	     'S_PLM_P140619_02022=1', #  Preventive Null Check For Avoiding Crash.
             'S_PLM_P140624_05001=1', #fix for null bitmap recieved from bitmapfromcachedResource
	     'SBROWSER_TEXT_ENCODING=1', #Fix for text encoding issue	 
#             'SBROWSER_SCROLL_POSITION_MISMATCH_FIX=1', #Fix for correcting the ScrollPosition in Webcore when ToolBar is visible(P140522-05221).
             'P140616_02189_IGNORE_TEXT_INDENTATION_FOR_SELECTION=1', #Ignore text indentation when calculating handler position
             'S_PLM_P140624_00882=1', #avoid sending callback to show selection handlers when selected text is empty
             'S_PLM_P140702_07649=1', #Preventive Null check for browser crash
             'S_PLM_P140703_05388=1', #disable hash-based text autosizing.
             'S_PLM_P140703_01695=1', #fix for spen text selection issue
             'S_PLM_P140711_07327=1', #fix for password field bar text
             'S_PLM_P140712_02787=1',  #fix for Text Selection issue
             'S_PLM_P140716_03409=1',  #preventive check for crash 
             'S_PLM_P140714_04554=1', #fix for text seletion tool tip delay issue
             'S_PLM_P140723_07592=1', #fix for text selection issue
             'S_PLM_P140726_00779=1', #removing logs from CONSOLE
             'S_PLM_P140816_02477=1', #Preventive check for null crash
             'S_PLM_P140906_03197=1', #preventive check for crash
             'S_PLM_P140903_00442=1', #null check current_frame is null
			 'S_PLM_P141126_02106=1', #null check form_frame is null
             'S_PLM_P140910_01988=1', #fix for appcache crash issue
             'S_JPN_CEDS_0489=1', #Reset data when input connection was changed.
             'S_VERIZON_IOT=1', #Verizon IOT test
             'S_ARROWKEY_FOCUS_NAVIGATION=1',#For spatial navigation
             'S_PLM_P140722_04533=1',#Render text box issue
#             'S_PLM_P140730_06767=1',#preventive null check for browser crash                         
             'S_AUTOFILL_PROFILE=1', #fix for duplicate autofill profile	
          #   'S_PLM_P140812_00507=1', #fix for password field composition
	    #FingerPrint WebLogin Flags++
            'S_FP_SUPPORT=1', #Propagating "FP additional_authentication" flag to Renderer.
            'S_FP_AUTOLOGIN_SUPPORT=1', #Autologin Support for WebLogin
            #'S_FP_DELAY_FORMSUBMIT=1',  #Delay Submit for AutoLogin by 500ms
            'S_FP_AVOID_AUTOLOGIN_FOR_HIDDEN_FORM=1', # Avoid AutoLogin if Form is not visible.
            'S_FP_AUTOLOGIN_FAILURE_ALERT=1', # Alert User for AutoLogin Failure.
            'S_FP_AUTOLOGIN_CAPTCHA_FIX=1', # Alert User for Captcha in the form and stop AutoLogin.
            'S_FP_HIDDEN_FORM_FIX=1', # Launch FP Screen and DO Autofill only when Form is visible.
            'S_FP_AVOID_SCREEN_AFTER_AUTOLOGIN=1',     #Fix to avoid FP screen after Autologin.
            'S_FP_AVOID_PASSWORD_SELECTION=1',     #Fix to avoid password selection while AutoLogin.
            'S_FP_WRONG_POPUP_FIX=1',     #Fix to avoid Remember password popup on Login Failure.
            'S_WRONG_PASSWORD_FACEBOOKPOPUPFIX=1', # Fix to Avoid Remember Password Popup When Wrong Credential is entered for FaceBook. 
            'S_FP_SIGNUP_POPUP_FIX=1',  #Avoid Remember Password Popup for SignUp Page.
            'S_FP_SIGNUP_AUTOFILL_FIX=1',  #Avoid Autofill for SignUp Page 
            'S_FP_WAIT_FOR_USERNAME_FIX=1',  #Fix for those forms which doesn't have any action specified. 
            'S_FP_INCOGNITO_MODE_SUPPORT=1',  #Support for Incognito Mode Autofill if the credential is saved in Normal Window. 
            'S_FP_DEFAULT_USERNAME_FIX=1',  #Fix for those forms which has default username value set during first time Login. 
            'S_FP_EMPTY_USERNAME_FIX=1',  #Fix for those forms which has text fields in between username field and password field.
            'S_FP_IFRAME_AUTOFILL_FIX=1',  # Allow Autofill for those forms are there in Iframe Also 
            'S_FP_MSSITES_AUTOFILL_FIX=1',  # Fix for MS related sites which does not support password autofill 
            'S_FP_COPY_OVER_PASSWORD_FIX=1',  # Fix for those forms which an operation to copy over previous password dose not work.
            'S_FP_COPY_OVER_PASSWORD_EXTENDED_FIX=1',  # Extended Fix of S_FP_COPY_OVER_PASSWORD_FIX.
            'S_FP_MULTI_PASSWORD_FIELD_FIX=1',  # Fix for those sites which have multiple password fields in their password form - InterPark.com
            'S_FP_MULTI_ACCOUNT_FIX=1',  # Multiple Account fixes
            'S_FP_COPY_OVER_USERNAME_FIX=1',  # # Fix for those forms which an operation to copy over previous username dose not work. 
            'S_FP_CHECKING_EMPTY_OR_INVALID_USERNAME=1',  # Do not show up remember popup if username contains a space character(For example, webmd.com).
            'S_FP_USERNAME_SPACE_CHARACTER_FIX=1',  # If User types username value with leading/trailing character, trim the username to avoid failure during Autofill.
            #'S_FP_NEW_TAB_FIX=1',  # Fix to show RPP for those sites where SignIn Page opens is New Tab and after SignIn it closes automatically.
            'S_FP_INVALID_EMAIL_USERNAME_FIX=1',  # On google.com, if user tries to login without full email adress, autofill will be failed due to different username.
            'S_FP_MIXED_CASE_USERNAME_FIX=1',  # On google.com, if user tries to login with mixed case username , Autofill Fails
            'S_AUTOCOMPLETE_IGNORE=1',  # # Extra Setting flag to Ignore Autocomplete attr. 
	    'S_DB_ENCRYPTION_256=1', # added new encryption logic api for password using AES256. 
            'S_AUTOCOMPLETE_ALERT_POPUP=1',  # # Show popup for those sites which has autocomplete off.
	        'S_UNITTEST_CRASH_FIX=1', #content_browser unittest segmentation crash fix 
            #FingerPrint WebLogin Flags--
            #'S_TRANSPORT_DIB_FOR_SOFT_BITMAP=1', # To use transport DIB for soft bitmaps
            'S_SCALING_FOR_SOFT_BITMAP=1', # To use partial scaling division for soft bitmaps in renderer and browser to reduce tranfer size
            'S_AUTOFILL_REMEMBER_FORM_FILL_DATA=1', # Setting for Autofill remember form fill data
            'S_HTML5CHECKDETECTOR=1',
            'S_HTTP_REQUEST_HEADERS=1', # set http request headers(includes uap)
	    'SBROWSER_QC_OPTIMIZATION=1',  #QCOM Patch support for disabling noscript tag content
#	    'SBROWSER_QC_OPTIMIZATION_PRECREATE_RENDERER=1', #QCOM Patch Precreate render process
            'S_INTUITIVE_HOVER=1',  #Intuitive hover (Hover Icon change)
            'SBROWSER_RINGMARK=1',  #Patch for ringmark test
            'S_SKIP_UNPREMULTIPLY_FOR_JPEG_IMAGE=1', #Improve canvas2D performance
            'SBROWSER_HIDE_URLBAR_HYBRID=1',  #HideURL related changes
            'SBROWSER_HIDE_URLBAR_EOP=1',  #HideURL related - To detect End of Page condition
            #Application side Feature Flag SBrowserFeatureFlag.mAppSideHideURLBarUICompositionFeature enable/disable state need to be in sync  
	    'SBROWSER_HIDE_URLBAR_UI_COMPOSITOR=1',  #Enables ToolBar Composition through UI Compositor, for HideUrlBar Feature. 
            'S_SET_SCROLL_TYPE=1', # set a scroll type
            #'S_PLM_P140823_03525=1', # Fix for urlbar flickering issue on hover scroll
            'SBROWSER_SOFTBITMAP_IMPL=1', #Soft bitmap related changes
            'S_FOP_SMOOTH_SCROLL=1', #Find On Page Smooth Scrolling Implementation
            'SBROWSER_MULTI_SELECTION=1',  #Multiple Selection related changes
            'S_MEDIAPLAYER_CONTENTVIDEOVIEW_ONSTART=1', # OnStart for video_view_ / P140517-02319
            'S_MEDIAPLAYER_CONTENTVIDEOVIEW_ONMEDIAINTERRUPTED=1', # OnMediaInterrupted for video_view_
            'S_MEDIAPLAYER_SBRCONTENTVIEWCOREIMPL_PAUSEVIDEO=1', # pauseVideo for activity onPause
            'S_MEDIAPLAYER_SBRCONTENTVIEWCOREIMPL_CREATEMEDIAPLAYERNOTIFICATION=1', # notification for android MediaPlayer created
            'S_MEDIAPLAYER_POWERSAVERBLOCKER_INVOKEDIDPAUSE=1', # InvokeDidPause for WakeLock issues
            'S_MEDIAPLAYER_INITIALTIME_SUPPORT=1', # implimentaion to support initialTime
            'S_MEDIAPLAYER_FULLSCREEN_CLOSEDCAPTION_SUPPORT=1', # toggling closed caption in full-screen mode
            'S_MEDIAPLAYER_TALKBACK_AUDIOFOCUS_FIX=1', # audio focus fix for talkback
            'S_MEDIAPLAYER_AUDIOFOCUS_MESSAGE_FIX', # audio focus message issue
            'S_MEDIAPLAYER_AUDIOFOCUS_GAIN_EVENT_FIX', # fix for update play status when audio focus is gained
            'S_MEDIAPLAYER_MEDIA_SOURCE_BUFFERING_SUPPORT=1', # support buffering update for MSP
            'S_MEDIAPLAYER_ONSTART_WEBCONTENTS_ISHIDDEN=1', # check if webcontents is hidden when OnStart is called
            'S_NETWORK_CONNECTION_COUNT=1', # connection count changes
            'S_AUTOFILL_SHOW_FIX=1',  # P140515-06692 Autofill is displayed in wrong place after rotation.
            'S_PLM_P140606_05128=1', # P140606-05128 crash in spdyproxy
            'S_SKIP_FLASH_FILE_DOWNLOAD', # Preventing unexpected download of flash content
			#Graphics
            'S_PLM_P140603_03145=1', # S_PLM_P140603_03145 crash ,merged open source code https://codereview.chromium.org/199443004
            'S_SUPPORT_XHTML_MP_MIME_TYPE=1', # add "application/vnd.wap.xhtml+xml" as a supported mime type
            'S_NETWORK_ERROR=1', #SBrowser network error ralated changes
            'S_SSL_ERROR=1', #SBrowser SSL error ralated changes
            'S_NET_EXPORT_LOG=1', #net-export log related changes
            'S_CERTIFICATE_DIALOG=1', #certicate dialog related changes
            'S_MULTISELECTION_BOUNDS=1', # flag to send extra bounds call incase of multiselection
            'SBROWSER_MULTIINSTANCE_TAB_DRAG_AND_DROP=1', # flag to enable multiinstance drag and drop
            'S_LOW_LES_ON_FASTSCROLL=1', # flag to support low res on fast scrolling
            'S_WORKER_POOL_PROFILE=1', # flag to track task posted to worker pool for execution
            'SBROWSER_EMOTICONS_DELETION_SUPPORT=1', # flag to fix deletion of emoticons using backspace key
#            'EGLIMAGE_RENDERTARGET = 1',#webxprt performance patch from QC with necessary modification from sbrowser side
            'S_PLM_P140621_01532=1', #  Anr happend opensource issue.  patch https://codereview.chromium.org/228663008 merged
            'SBROWSER_UI_COMPOSITOR_SET_BACKGROUND_COLOR=1',#Set UI Compositor background color
            'SBROWSER_PLM_P140701_01971=1',#To clear the selection while the page is loading
            'SBROWSER_PLM_P140704_01923=1',#Crash during stress test
            'SBROWSER_RTSP_IFRAME=1', #handling rtsp within iframe
            'S_HIDE_AUTOFILL_OPTIMIZATION=1', # HideAutofill optimization
            'S_DEFER_COMMIT_ON_GESTURE=1', # flag to defer commits on scroll
            'S_IGNORE_SCROLL_ACK_DISPOSITION=1', # flag to ignore scroll ack disposition
            'S_IGNORE_PINCH_ZOOM_ACK_DISPOSITION=1', # flag to ignore pinch zoom ack disposition
            'S_MODIFY_OVERSCROLL_CONDITION=1', # flag to modify overscroll condition.
            'S_USE_SYSTEM_PROXY_AUTH_CREDENTIAL=1', # flag to support wi-fi proxy auth feature
            'S_CLEAR_SESSION_CACHE_ON_VERSION_ERROR=1', # flag to use openssl's workaround solution
            'S_PLM_P140711_08590=1', # fix for skia crash when wbmp get null color table when draw in RGB565 canvas
            'SBROWSER_LOCALE_OPTIMIZATION=1', #The flag is used to enable/disable localization patch for power optimization.
            'S_RENDERER_START_EXIT_LOGGING=1', # flag to enable renderer process startup and exit logging
            'S_DO_NOT_KEEP_SESSION_COOKIE=1', # flag not to store session cookie
            'S_PLM_P140712_02056=1', #enabling chrome based missing plugin implementation back
            'S_PLM_P140718_08958=1', #added preventive checks for prerender_manager crash
            'S_SYSINFO_GETLANGUAGE=1', #get current android system language
            'S_TEXT_HIGHLIGHT_SELECTION_COLOR=1', # Background color of textselection as UX guide
			'SBROWSER_KERBEROS_FEATURE=1', # flag for SSO - Kerberos Integration
			'S_HTML5_CUSTOM_HANDLER_SUPPORT=1', # Support HTML5 custom handlers,
            'S_SYSINFO_GETANDROIDBUILDPDA=1', #get current android Build PDA
            'S_PLM_P140723_00458=1', #added prevent checks in databasethread
            'S_PLM_P140721_03683=1', #added support for setting touch points on enter key on focused node
            'S_PLM_P140730_04214=1',#added check equivalent to assert
            #'S_PLM_P140809_00188 =1',#flag to block sending a negative IME adapter value to JAVA layer.
            'S_PLM_P140811_02060 =1',#flag check assert for the fnode in skia draw.
            'S_PLM_P140811_03402=1', #consider unreachable url as original url because base url is not loaded when url is loaded with loadData
            'S_SCROLLBAR_LESS_UPDATE=1', #flag for scrollbar animation.
            #'S_PLM_P140806_02267=1',# Avoid top controls struck issue while using SPEN 
            'S_PLM_P140819_01865=1',#dlink text encoding issue
            'S_DATABASE_TRACKER=1', #fix database thread issue
            'S_PLM_P140830_01765=1', #default left handler position issue on image/video
            'S_KNOX_CERTIFICATE_POLICY=1', # knox certificate policy related code
            'S_PLM_P140903_00631 =1', #fix for drop down list in yahoo.com
            'S_PLM_P140909_03781 =1', #khmer glyphs not displayed due to overflow
            'S_CLEAR_SELECTION_ONAUTOFOCUS=1', #clearing the selection on auto focus
            'S_BUMPED_UP_FD_LIMIT=1', # flag to bumped up open file descriptor limit for browser process.
            'S_TRACE_ASHMEM_ALLOCATION_SOURCE=1', # flag to enable the ASHMEM allocations by giving names instead of ananymous allocations.
            'S_P140719_02604=1',#Preventive Null Check
            'S_P140916_01715=1',#touch event rects are not properly calculated corrected the same
            'S_PLM_P141016_02146 =1', # Change BreakingContext to use right dir for width
            'S_PLM_P141204_06444=1', # Fix Crash while history navigation
            'S_PLM_P141031_04993 =1', # external popup menu issue fix	
            'S_PLM_P141114_05571 =1', # Allow animations with delay to start on compositor
            'S_PLM_P141126_07375 =1', #added content type for WML mimetype
            'S_PLM_P141128_02771 =1', #turning the correct flag ON for load images
	     'S_IMAGE_SLIDE_ISSUE=1', #not able to slide images in 3g.qianlong.com
            #'NO_KEEP_PRERENDER_TILES  =1', # disable keep prerender tiles
            'S_FIT_TO_SCREEN=1', #supported fit to screen for tablet
            'S_PLM_P141212_04905=1', #fix for copying the image which is under iframe
        ],
      }],
      # FIXME: temporary build error fix for unittests
      ['enable_s_android_browser_unittest==1', {
        'defines': [
            'S_UNITTEST_SUPPORT=1',
        ],
      }],
      ['enable_qualcomm_v8==1', {
        'defines': [
            'S_ENABLE_QCOMSO=1',
        ],
      }],
    ],
  },
}
