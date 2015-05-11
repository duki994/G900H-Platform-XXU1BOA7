// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for interacting with frames.
// Multiply-included message file, hence no include guard.

#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/frame_message_enums.h"
#include "content/common/frame_param.h"
#include "content/common/navigation_gesture.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/page_state.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START FrameMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(FrameMsg_Navigate_Type::Value,
                          FrameMsg_Navigate_Type::NAVIGATE_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebContextMenuData::MediaType,
                          blink::WebContextMenuData::MediaTypeLast)

IPC_ENUM_TRAITS_MAX_VALUE(ui::MenuSourceType, ui::MENU_SOURCE_TYPE_LAST)

IPC_STRUCT_TRAITS_BEGIN(content::ContextMenuParams)
  IPC_STRUCT_TRAITS_MEMBER(media_type)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
  IPC_STRUCT_TRAITS_MEMBER(link_url)
  IPC_STRUCT_TRAITS_MEMBER(link_text)
  IPC_STRUCT_TRAITS_MEMBER(unfiltered_link_url)
  IPC_STRUCT_TRAITS_MEMBER(src_url)
  IPC_STRUCT_TRAITS_MEMBER(has_image_contents)
  IPC_STRUCT_TRAITS_MEMBER(page_url)
  IPC_STRUCT_TRAITS_MEMBER(keyword_url)
  IPC_STRUCT_TRAITS_MEMBER(frame_url)
  IPC_STRUCT_TRAITS_MEMBER(frame_id)
  IPC_STRUCT_TRAITS_MEMBER(frame_page_state)
  IPC_STRUCT_TRAITS_MEMBER(media_flags)
  IPC_STRUCT_TRAITS_MEMBER(selection_text)
  IPC_STRUCT_TRAITS_MEMBER(misspelled_word)
  IPC_STRUCT_TRAITS_MEMBER(misspelling_hash)
  IPC_STRUCT_TRAITS_MEMBER(dictionary_suggestions)
  IPC_STRUCT_TRAITS_MEMBER(speech_input_enabled)
  IPC_STRUCT_TRAITS_MEMBER(spellcheck_enabled)
  IPC_STRUCT_TRAITS_MEMBER(is_editable)
  IPC_STRUCT_TRAITS_MEMBER(writing_direction_default)
  IPC_STRUCT_TRAITS_MEMBER(writing_direction_left_to_right)
  IPC_STRUCT_TRAITS_MEMBER(writing_direction_right_to_left)
  IPC_STRUCT_TRAITS_MEMBER(edit_flags)
  IPC_STRUCT_TRAITS_MEMBER(security_info)
  IPC_STRUCT_TRAITS_MEMBER(frame_charset)
  IPC_STRUCT_TRAITS_MEMBER(referrer_policy)
  IPC_STRUCT_TRAITS_MEMBER(custom_context)
  IPC_STRUCT_TRAITS_MEMBER(custom_items)
  IPC_STRUCT_TRAITS_MEMBER(source_type)
#if defined(OS_ANDROID)
  IPC_STRUCT_TRAITS_MEMBER(selection_start)
  IPC_STRUCT_TRAITS_MEMBER(selection_end)
#endif
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::CustomContextMenuContext)
  IPC_STRUCT_TRAITS_MEMBER(is_pepper_menu)
  IPC_STRUCT_TRAITS_MEMBER(request_id)
  IPC_STRUCT_TRAITS_MEMBER(render_widget_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(FrameHostMsg_DidFailProvisionalLoadWithError_Params)
  // The frame ID for the failure report.
  IPC_STRUCT_MEMBER(int64, frame_id)
  // The WebFrame's uniqueName().
  IPC_STRUCT_MEMBER(base::string16, frame_unique_name)
  // True if this is the top-most frame.
  IPC_STRUCT_MEMBER(bool, is_main_frame)
  // Error code as reported in the DidFailProvisionalLoad callback.
  IPC_STRUCT_MEMBER(int, error_code)
  // An error message generated from the error_code. This can be an empty
  // string if we were unable to find a meaningful description.
  IPC_STRUCT_MEMBER(base::string16, error_description)
  // The URL that the error is reported for.
  IPC_STRUCT_MEMBER(GURL, url)
  // True if the failure is the result of navigating to a POST again
  // and we're going to show the POST interstitial.
  IPC_STRUCT_MEMBER(bool, showing_repost_interstitial)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(content::FrameNavigateParams)
  IPC_STRUCT_TRAITS_MEMBER(page_id)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(base_url)
  IPC_STRUCT_TRAITS_MEMBER(referrer)
  IPC_STRUCT_TRAITS_MEMBER(transition)
  IPC_STRUCT_TRAITS_MEMBER(redirects)
  IPC_STRUCT_TRAITS_MEMBER(should_update_history)
  IPC_STRUCT_TRAITS_MEMBER(searchable_form_url)
  IPC_STRUCT_TRAITS_MEMBER(searchable_form_encoding)
  IPC_STRUCT_TRAITS_MEMBER(contents_mime_type)
  IPC_STRUCT_TRAITS_MEMBER(socket_address)
IPC_STRUCT_TRAITS_END()

// Parameters structure for FrameHostMsg_DidCommitProvisionalLoad, which has
// too many data parameters to be reasonably put in a predefined IPC message.
IPC_STRUCT_BEGIN_WITH_PARENT(FrameHostMsg_DidCommitProvisionalLoad_Params,
                             content::FrameNavigateParams)
  IPC_STRUCT_TRAITS_PARENT(content::FrameNavigateParams)
  // The frame ID for this navigation. The frame ID uniquely identifies the
  // frame the navigation happened in for a given renderer.
  IPC_STRUCT_MEMBER(int64, frame_id)

  // The WebFrame's uniqueName().
  IPC_STRUCT_MEMBER(base::string16, frame_unique_name)

  // Information regarding the security of the connection (empty if the
  // connection was not secure).
  IPC_STRUCT_MEMBER(std::string, security_info)

  // The gesture that initiated this navigation.
  IPC_STRUCT_MEMBER(content::NavigationGesture, gesture)

  // True if this was a post request.
  IPC_STRUCT_MEMBER(bool, is_post)

  // The POST body identifier. -1 if it doesn't exist.
  IPC_STRUCT_MEMBER(int64, post_id)

  // Whether the frame navigation resulted in no change to the documents within
  // the page. For example, the navigation may have just resulted in scrolling
  // to a named anchor.
  IPC_STRUCT_MEMBER(bool, was_within_same_page)

  // The status code of the HTTP request.
  IPC_STRUCT_MEMBER(int, http_status_code)

  // True if the connection was proxied.  In this case, socket_address
  // will represent the address of the proxy, rather than the remote host.
  IPC_STRUCT_MEMBER(bool, was_fetched_via_proxy)

  // Serialized history item state to store in the navigation entry.
  IPC_STRUCT_MEMBER(content::PageState, page_state)

  // Original request's URL.
  IPC_STRUCT_MEMBER(GURL, original_request_url)

  // User agent override used to navigate.
  IPC_STRUCT_MEMBER(bool, is_overriding_user_agent)

  // Notifies the browser that for this navigation, the session history was
  // successfully cleared.
  IPC_STRUCT_MEMBER(bool, history_list_was_cleared)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(FrameMsg_Navigate_Params)
  // The page_id for this navigation, or -1 if it is a new navigation.  Back,
  // Forward, and Reload navigations should have a valid page_id.  If the load
  // succeeds, then this page_id will be reflected in the resultant
  // FrameHostMsg_DidCommitProvisionalLoad message.
  IPC_STRUCT_MEMBER(int32, page_id)

  // If page_id is -1, then pending_history_list_offset will also be -1.
  // Otherwise, it contains the offset into the history list corresponding to
  // the current navigation.
  IPC_STRUCT_MEMBER(int, pending_history_list_offset)

  // Informs the RenderView of where its current page contents reside in
  // session history and the total size of the session history list.
  IPC_STRUCT_MEMBER(int, current_history_list_offset)
  IPC_STRUCT_MEMBER(int, current_history_list_length)

  // Informs the RenderView the session history should be cleared. In that
  // case, the RenderView needs to notify the browser that the clearing was
  // succesful when the navigation commits.
  IPC_STRUCT_MEMBER(bool, should_clear_history_list)

  // The URL to load.
  IPC_STRUCT_MEMBER(GURL, url)

  // Base URL for use in WebKit's SubstituteData.
  // Is only used with data: URLs.
  IPC_STRUCT_MEMBER(GURL, base_url_for_data_url)

  // History URL for use in WebKit's SubstituteData.
  // Is only used with data: URLs.
  IPC_STRUCT_MEMBER(GURL, history_url_for_data_url)

  // The URL to send in the "Referer" header field. Can be empty if there is
  // no referrer.
  IPC_STRUCT_MEMBER(content::Referrer, referrer)

  // Any redirect URLs that occurred before |url|. Useful for cross-process
  // navigations; defaults to empty.
  IPC_STRUCT_MEMBER(std::vector<GURL>, redirects)

  // The type of transition.
  IPC_STRUCT_MEMBER(content::PageTransition, transition)

  // Informs the RenderView the pending navigation should replace the current
  // history entry when it commits. This is used for cross-process redirects so
  // the transferred navigation can recover the navigation state.
  IPC_STRUCT_MEMBER(bool, should_replace_current_entry)

  // Opaque history state (received by ViewHostMsg_UpdateState).
  IPC_STRUCT_MEMBER(content::PageState, page_state)

  // Type of navigation.
  IPC_STRUCT_MEMBER(FrameMsg_Navigate_Type::Value, navigation_type)

  // The time the request was created
  IPC_STRUCT_MEMBER(base::Time, request_time)

  // Extra headers (separated by \n) to send during the request.
  IPC_STRUCT_MEMBER(std::string, extra_headers)

  // The following two members identify a previous request that has been
  // created before this navigation is being transferred to a new render view.
  // This serves the purpose of recycling the old request.
  // Unless this refers to a transferred navigation, these values are -1 and -1.
  IPC_STRUCT_MEMBER(int, transferred_request_child_id)
  IPC_STRUCT_MEMBER(int, transferred_request_request_id)

  // Whether or not we should allow the url to download.
  IPC_STRUCT_MEMBER(bool, allow_download)

  // Whether or not the user agent override string should be used.
  IPC_STRUCT_MEMBER(bool, is_overriding_user_agent)

  // True if this was a post request.
  IPC_STRUCT_MEMBER(bool, is_post)

  // If is_post is true, holds the post_data information from browser. Empty
  // otherwise.
  IPC_STRUCT_MEMBER(std::vector<unsigned char>, browser_initiated_post_data)

  // Whether or not this url should be allowed to access local file://
  // resources.
  IPC_STRUCT_MEMBER(bool, can_load_local_resources)

  // If not empty, which frame to navigate.
  IPC_STRUCT_MEMBER(std::string, frame_to_navigate)

  // The navigationStart time to expose to JS for this navigation.
  IPC_STRUCT_MEMBER(base::TimeTicks, browser_navigation_start)
IPC_STRUCT_END()

// -----------------------------------------------------------------------------
// Messages sent from the browser to the renderer.

// When HW accelerated buffers are swapped in an out-of-process child frame
// renderer, the message is forwarded to the embedding frame to notify it of
// a new texture available for compositing. When the buffer has finished
// presenting, a FrameHostMsg_BuffersSwappedACK should be sent back to
// gpu host that produced this buffer.
//
// This is used in the non-ubercomp HW accelerated compositing path.
IPC_MESSAGE_ROUTED1(FrameMsg_BuffersSwapped,
                    FrameMsg_BuffersSwapped_Params /* params */)

// Notifies the embedding frame that a new CompositorFrame is ready to be
// presented. When the frame finishes presenting, a matching
// FrameHostMsg_CompositorFrameSwappedACK should be sent back to the
// RenderViewHost that was produced the CompositorFrame.
//
// This is used in the ubercomp compositing path.
IPC_MESSAGE_ROUTED1(FrameMsg_CompositorFrameSwapped,
                    FrameMsg_CompositorFrameSwapped_Params /* params */)

// Notifies the embedding frame that the process rendering the child frame's
// contents has terminated.
IPC_MESSAGE_ROUTED0(FrameMsg_ChildFrameProcessGone)

// Sent in response to a FrameHostMsg_ContextMenu to let the renderer know that
// the menu has been closed.
IPC_MESSAGE_ROUTED1(FrameMsg_ContextMenuClosed,
                    content::CustomContextMenuContext /* custom_context */)

// Executes custom context menu action that was provided from Blink.
IPC_MESSAGE_ROUTED2(FrameMsg_CustomContextMenuAction,
                    content::CustomContextMenuContext /* custom_context */,
                    unsigned /* action */)

// Tells the renderer to perform the specified navigation, interrupting any
// existing navigation.
IPC_MESSAGE_ROUTED1(FrameMsg_Navigate, FrameMsg_Navigate_Params)

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// Sent by the renderer when a child frame is created in the renderer. The
// |parent_frame_id| and |frame_id| are NOT routing ids. They are
// renderer-allocated identifiers used for tracking a frame's creation.
//
// Each of these messages will have a corresponding FrameHostMsg_Detach message
// sent when the frame is detached from the DOM.
//
// TOOD(ajwong): replace parent_render_frame_id and frame_id with just the
// routing ids.
IPC_SYNC_MESSAGE_CONTROL4_1(FrameHostMsg_CreateChildFrame,
                            int32 /* parent_render_frame_id */,
                            int64 /* parent_frame_id */,
                            int64 /* frame_id */,
                            std::string /* frame_name */,
                            int /* new_render_frame_id */)

// Sent by the renderer to the parent RenderFrameHost when a child frame is
// detached from the DOM.
IPC_MESSAGE_ROUTED2(FrameHostMsg_Detach,
                    int64 /* parent_frame_id */,
                    int64 /* frame_id */)

// Sent when the renderer starts a provisional load for a frame.
IPC_MESSAGE_ROUTED4(FrameHostMsg_DidStartProvisionalLoadForFrame,
                    int64 /* frame_id */,
                    int64 /* parent_frame_id */,
                    bool /* true if it is the main frame */,
                    GURL /* url */)

// Sent when the renderer fails a provisional load with an error.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidFailProvisionalLoadWithError,
                    FrameHostMsg_DidFailProvisionalLoadWithError_Params)

// Sent when a provisional load on the main frame redirects.
IPC_MESSAGE_ROUTED3(FrameHostMsg_DidRedirectProvisionalLoad,
                    int /* page_id */,
                    GURL /* source_url*/,
                    GURL /* target_url */)

// Notifies the browser that a frame in the view has changed. This message
// has a lot of parameters and is packed/unpacked by functions defined in
// render_messages.h.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidCommitProvisionalLoad,
                    FrameHostMsg_DidCommitProvisionalLoad_Params)

// Notifies the browser that a document has been loaded.
IPC_MESSAGE_ROUTED1(FrameHostMsg_DidFinishDocumentLoad,
                    int64 /* frame_id */)

IPC_MESSAGE_ROUTED5(FrameHostMsg_DidFailLoadWithError,
                    int64 /* frame_id */,
                    GURL /* validated_url */,
                    bool /* is_main_frame */,
                    int /* error_code */,
                    base::string16 /* error_description */)

// Sent when the renderer starts loading the page. This corresponds to
// Blink's notion of the throbber starting. Note that sometimes you may get
// duplicates of these during a single load.
IPC_MESSAGE_ROUTED0(FrameHostMsg_DidStartLoading)

// Sent when the renderer is done loading a page. This corresponds to Blink's
// notion of the throbber stopping.
IPC_MESSAGE_ROUTED0(FrameHostMsg_DidStopLoading)

// Sent to the browser when the renderer detects it is blocked on a pepper
// plugin message for too long. This is also sent when it becomes unhung
// (according to the value of is_hung). The browser can give the user the
// option of killing the plugin.
IPC_MESSAGE_ROUTED3(FrameHostMsg_PepperPluginHung,
                    int /* plugin_child_id */,
                    base::FilePath /* path */,
                    bool /* is_hung */)

// Sent by the renderer process to indicate that a plugin instance has crashed.
// Note: |plugin_pid| should not be trusted. The corresponding process has
// probably died. Moreover, the ID may have been reused by a new process. Any
// usage other than displaying it in a prompt to the user is very likely to be
// wrong.
IPC_MESSAGE_ROUTED2(FrameHostMsg_PluginCrashed,
                    base::FilePath /* plugin_path */,
                    base::ProcessId /* plugin_pid */)

// Return information about a plugin for the given URL and MIME
// type. If there is no matching plugin, |found| is false.
// |actual_mime_type| is the actual mime type supported by the
// found plugin.
IPC_SYNC_MESSAGE_CONTROL4_3(FrameHostMsg_GetPluginInfo,
                            int /* render_frame_id */,
                            GURL /* url */,
                            GURL /* page_url */,
                            std::string /* mime_type */,
                            bool /* found */,
                            content::WebPluginInfo /* plugin info */,
                            std::string /* actual_mime_type */)

// A renderer sends this to the browser process when it wants to
// create a plugin.  The browser will create the plugin process if
// necessary, and will return a handle to the channel on success.
// On error an empty string is returned.
IPC_SYNC_MESSAGE_CONTROL4_2(FrameHostMsg_OpenChannelToPlugin,
                            int /* render_frame_id */,
                            GURL /* url */,
                            GURL /* page_url */,
                            std::string /* mime_type */,
                            IPC::ChannelHandle /* channel_handle */,
                            content::WebPluginInfo /* info */)

// Acknowledge that we presented a HW buffer and provide a sync point
// to specify the location in the command stream when the compositor
// is no longer using it.
//
// See FrameMsg_BuffersSwapped.
IPC_MESSAGE_ROUTED1(FrameHostMsg_BuffersSwappedACK,
                    FrameHostMsg_BuffersSwappedACK_Params /* params */)

// Acknowledge that we presented an ubercomp frame.
//
// See FrameMsg_CompositorFrameSwapped
IPC_MESSAGE_ROUTED1(FrameHostMsg_CompositorFrameSwappedACK,
                    FrameHostMsg_CompositorFrameSwappedACK_Params /* params */)

// Indicates that the current frame has swapped out, after a SwapOut message.
IPC_MESSAGE_ROUTED0(FrameHostMsg_SwapOut_ACK)

IPC_MESSAGE_ROUTED1(FrameHostMsg_ReclaimCompositorResources,
                    FrameHostMsg_ReclaimCompositorResources_Params /* params */)

// Forwards an input event to a child.
// TODO(nick): Temporary bridge, revisit once the browser process can route
// input directly to subframes. http://crbug.com/339659
IPC_MESSAGE_ROUTED1(FrameHostMsg_ForwardInputEvent,
                    IPC::WebInputEventPointer /* event */)

// Instructs the frame to swap out for a cross-site transition, including
// running the unload event handler. Expects a SwapOut_ACK message when
// finished.
IPC_MESSAGE_ROUTED0(FrameMsg_SwapOut)

// Used to tell the parent that the user right clicked on an area of the
// content area, and a context menu should be shown for it. The params
// object contains information about the node(s) that were selected when the
// user right clicked.
IPC_MESSAGE_ROUTED1(FrameHostMsg_ContextMenu, content::ContextMenuParams)
