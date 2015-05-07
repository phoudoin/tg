/*
    This file is part of telegram-cli.

    Telegram-cli is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Telegram-cli is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this telegram-cli.  If not, see <http://www.gnu.org/licenses/>.

    Copyright Vitaly Valtman 2013-2015
    Copyright Vincent Castellano 2015
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef USE_PYTHON
#include "python-tg.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <libgen.h>


#include <Python.h>
// #include <datetime.h>

#ifdef EVENT_V2
#include <event2/event.h>
#else
#include <event.h>
#include "event-old.h"
#endif

//#include "interface.h"
//#include "auto/constants.h"
#include <tgl/tgl.h>
#include "interface.h"

#include <assert.h>
extern int verbosity;
extern struct tgl_state *TLS;

static int have_file;

// PyDateTime_IMPORT; 


// Python update function callables
PyObject *_py_binlog_end;
PyObject *_py_diff_end;
PyObject *_py_our_id;
PyObject *_py_new_msg;
PyObject *_py_secret_chat_update;
PyObject *_py_user_update;
PyObject *_py_chat_update;

PyObject* get_user (tgl_peer_t *P);
PyObject* get_peer (tgl_peer_id_t id, tgl_peer_t *P);

void py_add_string_field (PyObject* dict, char *name, const char *value) {
  assert (PyDict_Check(dict));
  assert (name && strlen (name));
  if (!value || !strlen (value)) { return; }
  PyDict_SetItemString (dict, name, PyString_FromString(value));
}

void py_add_string_field_arr (PyObject* list, int num, const char *value) {
  assert(PyList_Check(list));
  if (!value || !strlen (value)) { return; }
  if(num >= 0)
    PyList_SetItem (list, num, PyString_FromString (value));
  else // Append
    PyList_Append  (list, PyString_FromString (value));
}

void py_add_num_field (PyObject* dict, const char *name, double value) {
  assert (PyDict_Check(dict));
  assert (name && strlen (name));
  PyDict_SetItemString (dict, name, PyFloat_FromDouble(value));
}


#if 0
PyObject* get_datetime(long datetime)
{
  return PyDateTime_FromTimestamp(Py_BuildValue("(O)", PyLong_FromLong(datetime)));
}
#endif


PyObject* get_tgl_peer_type (int x) {
  PyObject *type;

  switch (x) {
  case TGL_PEER_USER:
    type = PyString_FromString("user");
    break;
  case TGL_PEER_CHAT:
    type = PyString_FromString("chat");
    break;
  case TGL_PEER_ENCR_CHAT:
    type = PyString_FromString("encr_chat");
    break;
  default:
    assert (0);
  }

  return type;
}

PyObject* get_user (tgl_peer_t *P) {
  PyObject *user;
  
  user = PyDict_New();
  if(user == NULL)
    assert(0); // TODO handle python exception

  py_add_string_field (user, "first_name",      P->user.first_name);
  py_add_string_field (user, "last_name",       P->user.last_name);
  py_add_string_field (user, "real_first_name", P->user.real_first_name);
  py_add_string_field (user, "real_last_name",  P->user.real_last_name);
  py_add_string_field (user, "phone",           P->user.phone);
  if (P->user.access_hash) {
    py_add_num_field (user, "access_hash",   1);
  }
  if (P->user.username) {
    py_add_string_field (user, "username", P->user.username);
  }

  return user;
}

PyObject* get_chat (tgl_peer_t *P) {
  PyObject *chat, *members;

  chat = PyDict_New();
  if(chat == NULL)
    assert(0); // TODO handle python exception

  assert (P->chat.title);

  py_add_string_field (chat, "title",       P->chat.title);
  py_add_num_field (chat, "members_num", P->chat.users_num);
  if (P->chat.user_list) {
    members = PyList_New(P->chat.users_num);
    if(members == NULL)
      assert(0); // TODO handle python exception

    int i;
    for (i = 0; i < P->chat.users_num; i++) {
      tgl_peer_id_t id = TGL_MK_USER (P->chat.user_list[i].user_id);
      PyList_SetItem (members, i, get_peer(id, tgl_peer_get (TLS, id)));
    }
    PyDict_SetItemString (chat, "members", members);
  }

  return chat;
}

PyObject* get_encr_chat (tgl_peer_t *P) {
  PyObject *encr_chat, *user;

  encr_chat = PyDict_New();
  if(encr_chat == NULL)
    assert(0); // TODO handle python exception

  user = get_peer (TGL_MK_USER (P->encr_chat.user_id), tgl_peer_get (TLS, TGL_MK_USER (P->encr_chat.user_id)));
  PyDict_SetItemString (encr_chat, "user", user);
  
  return encr_chat;
}

PyObject* get_update_types (unsigned flags) {
  PyObject* types;
  types = PyList_New(0); 
  if(types == NULL)
    assert(0); // TODO handle python exception
  
  if (flags & TGL_UPDATE_CREATED) {
    py_add_string_field_arr(types, -1, "created");
  }  
  if (flags & TGL_UPDATE_DELETED) {
    py_add_string_field_arr(types, -1, "deleted");
  }  
  if (flags & TGL_UPDATE_PHONE) {
    py_add_string_field_arr(types, -1, "phone");
  }
  if (flags & TGL_UPDATE_CONTACT) {
    py_add_string_field_arr(types, -1, "contact");
  }
  if (flags & TGL_UPDATE_PHOTO) {
    py_add_string_field_arr(types, -1, "photo");
  }
  if (flags & TGL_UPDATE_BLOCKED) {
    py_add_string_field_arr(types, -1, "blocked");
  }
  if (flags & TGL_UPDATE_REAL_NAME) {
    py_add_string_field_arr(types, -1, "real_name");
  }
  if (flags & TGL_UPDATE_NAME) {
    py_add_string_field_arr(types, -1, "name");
  }
  if (flags & TGL_UPDATE_REQUESTED) {
    py_add_string_field_arr(types, -1, "requested");
  }
  if (flags & TGL_UPDATE_WORKING) {
    py_add_string_field_arr(types, -1, "working");
  }
  if (flags & TGL_UPDATE_FLAGS) {
    py_add_string_field_arr(types, -1, "flags");
  }
  if (flags & TGL_UPDATE_TITLE) {
    py_add_string_field_arr(types, -1, "title");
  }
  if (flags & TGL_UPDATE_ADMIN) {
    py_add_string_field_arr(types, -1, "admin");
  }
  if (flags & TGL_UPDATE_MEMBERS) {
    py_add_string_field_arr(types, -1, "members");
  }
  if (flags & TGL_UPDATE_ACCESS_HASH) {
    py_add_string_field_arr(types, -1, "access_hash");
  }
  if (flags & TGL_UPDATE_USERNAME) {
    py_add_string_field_arr(types, -1, "username");
  }
  return types;
}

PyObject* get_peer (tgl_peer_id_t id, tgl_peer_t *P) {
  PyObject *peer;

  peer = PyDict_New();
  if(peer == NULL)
    assert(0); // TODO handle python exception;

  PyDict_SetItemString (peer, "type_str", get_tgl_peer_type (tgl_get_peer_type(id)));
  PyDict_SetItemString (peer, "type", PyInt_FromLong(tgl_get_peer_type(id)));
  PyDict_SetItemString (peer, "id", PyInt_FromLong(tgl_get_peer_id(id)));

  if (!P || !(P->flags & FLAG_CREATED)) {
    PyObject *name;

    static char s[100];
    switch (tgl_get_peer_type (id)) {
    case TGL_PEER_USER:
      sprintf (s, "user#%d", tgl_get_peer_id (id));
      break;
    case TGL_PEER_CHAT:
      sprintf (s, "chat#%d", tgl_get_peer_id (id));
      break;
    case TGL_PEER_ENCR_CHAT:
      sprintf (s, "encr_chat#%d", tgl_get_peer_id (id));
      break;
    default:
      assert (0);
    }
    
    name = PyDict_New();
    if(name == NULL)
      assert(0); // TODO handle python exception;

    PyDict_SetItemString (name, "print_name", PyString_FromString(s));
    PyDict_SetItemString (peer, "peer", name);
  } else {
    PyObject *peer_obj;
    
    switch (tgl_get_peer_type (id)) {
    case TGL_PEER_USER:
      peer_obj = get_user (P);
      break;
    case TGL_PEER_CHAT:
      peer_obj = get_chat (P);
      break;
    case TGL_PEER_ENCR_CHAT:
      peer_obj = get_encr_chat (P);
      break;
    default:
      assert (0);
    }
    PyDict_SetItemString (peer, "peer", peer_obj);
  }

  return peer;
}

PyObject* get_media (struct tgl_message_media *M) {
  PyObject *media;

  media = PyDict_New();
  if(media == NULL)
    assert(0); // TODO handle python exception

  switch (M->type) {
  case tgl_message_media_photo:
  case tgl_message_media_photo_encr:
    py_add_string_field (media, "type", "photo");
    py_add_string_field (media, "caption", M->caption);
    break;
  case tgl_message_media_document:
  case tgl_message_media_document_encr:
    py_add_string_field (media, "type", "document");
    break;
  case tgl_message_media_unsupported:
    py_add_string_field (media, "type", "unsupported");
    break;
  case tgl_message_media_geo:
    py_add_string_field (media, "type", "geo");
    py_add_num_field (media, "longitude", M->geo.longitude);
    py_add_num_field (media, "latitude", M->geo.latitude);
    break;
  case tgl_message_media_contact:
    py_add_string_field (media, "type", "contact");
    py_add_string_field (media, "phone", M->phone);
    py_add_string_field (media, "first_name", M->first_name);
    py_add_string_field (media, "last_name", M->last_name);
    py_add_num_field (media, "user_id", M->user_id);
    break;
  case tgl_message_media_webpage:
    py_add_string_field (media, "type", "webpage");
    py_add_string_field (media, "url", M->webpage->url);
    py_add_string_field (media, "title", M->webpage->title);
    py_add_string_field (media, "description", M->webpage->description);
    py_add_string_field (media, "author", M->webpage->author);
    break;
  case tgl_message_media_venue:
    py_add_string_field (media, "type", "venue");
    py_add_num_field (media, "longitude", M->venue.geo.longitude);
    py_add_num_field (media, "latitude", M->venue.geo.latitude);
    py_add_string_field (media, "title", M->venue.title);
    py_add_string_field (media, "address", M->venue.address);
    py_add_string_field (media, "provider", M->venue.provider);
    py_add_string_field (media, "venue_id", M->venue.venue_id);
    break;
  default:
    py_add_string_field (media, "type", "unknown");
  }

  return media;
}


PyObject* get_service (struct tgl_message* M) {
  assert (M);
  PyObject *msg;

  msg = PyDict_New();
  if(msg == NULL)
    assert(0); // TODO handle python exception

  // TODO
  switch (M->action.type) {
  case tgl_message_action_geo_chat_create:
    py_add_string_field (msg, "type", "geo_created");
    break;
  case tgl_message_action_geo_chat_checkin:
    py_add_string_field (msg, "type", "geo_checkin");
    break;
  case tgl_message_action_chat_create:
    py_add_string_field (msg, "type", "chat_created");
    py_add_string_field (msg, "title", M->action.title);
    break;
  case tgl_message_action_chat_edit_title:
    py_add_string_field (msg, "type", "chat_rename");
    py_add_string_field (msg, "title", M->action.title);
    break;
  case tgl_message_action_chat_edit_photo:
    py_add_string_field (msg, "type", "chat_change_photo");
    break;
  case tgl_message_action_chat_delete_photo:
    py_add_string_field (msg, "type", "chat_delete_photo");
    break;
  case tgl_message_action_chat_add_user:
    py_add_string_field (msg, "type", "chat_add_user");
    PyDict_SetItemString(msg, "user", get_peer(M->action.user, tgl_peer_get (TLS, M->action.user)));
    break;
  case tgl_message_action_chat_add_user_by_link:
    py_add_string_field (msg, "type", "chat_add_user_link");
    PyDict_SetItemString(msg, "link_issuer", get_peer(M->action.user, tgl_peer_get (TLS, M->action.user)));
    break;
  case tgl_message_action_chat_delete_user:
    py_add_string_field (msg, "type", "chat_del_user");
    PyDict_SetItemString(msg, "user", get_peer(M->action.user, tgl_peer_get (TLS, M->action.user)));
    break;
  case tgl_message_action_set_message_ttl:
    py_add_string_field (msg, "type", "set_ttl");
    py_add_num_field (msg, "ttl", M->action.ttl);
    break;
  case tgl_message_action_read_messages:
    py_add_string_field (msg, "type", "read");
    py_add_num_field (msg, "count", M->action.read_count);
    break;
  case tgl_message_action_delete_messages:
    py_add_string_field (msg, "type", "delete");
    py_add_num_field (msg, "count", M->action.delete_count);
    break;
  case tgl_message_action_screenshot_messages:
    py_add_string_field (msg, "type", "screenshot");
    py_add_num_field (msg, "count", M->action.screenshot_count);
    break;
  case tgl_message_action_flush_history:
    py_add_string_field (msg, "type", "flush");
    break;
  case tgl_message_action_resend:
    py_add_string_field (msg, "type", "resend");
    break;
  case tgl_message_action_notify_layer:
    py_add_string_field (msg, "type", "set_layer");
    py_add_num_field (msg, "layer", M->action.layer);
    break;
  case tgl_message_action_typing:    
    py_add_string_field (msg, "type", "typing");
    break;
  case tgl_message_action_noop:
    py_add_string_field (msg, "type", "nop");
    break;
  case tgl_message_action_request_key:
    py_add_string_field (msg, "type", "request_rekey");
    break;
  case tgl_message_action_accept_key:
    py_add_string_field (msg, "type", "accept_rekey");
    break;
  case tgl_message_action_commit_key:
    py_add_string_field (msg, "type", "commit_rekey");
    break;
  case tgl_message_action_abort_key:
    py_add_string_field (msg, "type", "abort_rekey");
    break;
  default:
    py_add_string_field (msg, "type", "???");
    break;
  }

  return msg;
}


PyObject* get_message (struct tgl_message *M) {  
  assert (M);
  PyObject *msg;

  msg = PyDict_New();
  if(msg == NULL)
    assert(0); // TODO handle python exception

  static char s[30];
  snprintf (s, 30, "%lld", M->id);
  py_add_string_field (msg, "id", s);
  if (!(M->flags & FLAG_CREATED)) { 
    return msg; 
  }

  py_add_num_field (msg, "flags", M->flags);
  if (M->flags & TGLMF_MENTION) {
    PyDict_SetItemString(msg, "mention", Py_True);
  }

#if 0
  if (M->reply_id) {
    py_add_num_field ("reply_id", M->reply_id);
    struct tgl_message *MR = tgl_message_get (TLS, M->reply_id);
    // Message details available only within session for now
    if (MR) {
      PyDict_SetItemString(msg, "reply_to", get_message(MR));
    }
  }
#endif
  if (tgl_get_peer_type (M->fwd_from_id)) {
    PyDict_SetItemString(msg, "fwd_from", get_peer(M->fwd_from_id, tgl_peer_get (TLS, M->fwd_from_id)));
    py_add_num_field (msg, "fwd_date", M->fwd_date);
  }
 
  PyDict_SetItemString(msg, "from",    get_peer(M->from_id, tgl_peer_get (TLS, M->from_id)));
  PyDict_SetItemString(msg, "to",      get_peer(M->to_id, tgl_peer_get (TLS, M->to_id)));
  PyDict_SetItemString(msg, "out",     ((M->flags & TGLMF_OUT) != 0) ? Py_True : Py_False);
  PyDict_SetItemString(msg, "unread",  ((M->flags & TGLMF_UNREAD) != 0) ? Py_True : Py_False);
  PyDict_SetItemString(msg, "service", ((M->flags & TGLMF_SERVICE) != 0) ? Py_True : Py_False);
  PyDict_SetItemString(msg, "date",    PyLong_FromLong(M->date)); // TODO put this into PyDate object

  if (!(M->flags & TGLMF_SERVICE)) {  
    if (M->message_len && M->message) {
      PyDict_SetItemString(msg, "text", PyString_FromStringAndSize(M->message, M->message_len));
    }
    if (M->media.type && M->media.type != tgl_message_media_none) {
      PyDict_SetItemString(msg, "media", get_media(&M->media));  
    }
  } else {
    PyDict_SetItemString(msg, "action", get_service(M));  
  }

  return msg;
}


void check_result(PyObject* result)
{
  if (result == NULL) {  
    PyErr_Print();
  } else {
    logprintf ("python: %s\n", PyString_AsString(result));
  }
  Py_DECREF(result);
}


void py_binlog_end (void) {
  if (!have_file) { return; }

  PyObject *arglist, *result;

  arglist = Py_BuildValue("()");
  result = PyEval_CallObject(_py_binlog_end, arglist);
  Py_DECREF(arglist);
  check_result(result);
}

void py_diff_end (void) {
  if (!have_file) { return; }

  PyObject *arglist, *result;

  arglist = Py_BuildValue("()");
  result = PyEval_CallObject(_py_diff_end, arglist);
  Py_DECREF(arglist);
  check_result(result);
}

void py_our_id (int id) {
  if (!have_file) { return; }

  PyObject *arglist, *result;

  arglist = Py_BuildValue("(i)", id);
  result = PyEval_CallObject(_py_our_id, arglist);
  Py_DECREF(arglist);
  check_result(result);
}

void py_new_msg (struct tgl_message *M) {
  if (!have_file) { return; }
  PyObject *msg;
  PyObject *arglist, *result;

  msg = get_message (M);

  arglist = Py_BuildValue("(O)", msg);
  result = PyEval_CallObject(_py_new_msg, arglist);
  Py_DECREF(arglist);
  check_result(result);
}

void py_secret_chat_update (struct tgl_secret_chat *C, unsigned flags) {
  if (!have_file) { return; }
  PyObject *peer, *types;
  PyObject *arglist, *result; 

  peer = get_peer (C->id, (void *)C);
  types = get_update_types (flags);

  arglist = Py_BuildValue("(OO)", peer, types);
  result = PyEval_CallObject(_py_secret_chat_update, arglist);
  Py_DECREF(arglist);
  check_result(result);
}


void py_user_update (struct tgl_user *U, unsigned flags) {
  if (!have_file) { return; }
  PyObject *peer, *types;
  PyObject *arglist, *result;

  peer = get_peer (U->id, (void *)U);
  types = get_update_types (flags);

  arglist = Py_BuildValue("(OO)", peer, types);
  result = PyEval_CallObject(_py_user_update, arglist);
  Py_DECREF(arglist);
  check_result(result);
}

void py_chat_update (struct tgl_chat *C, unsigned flags) {
  if (!have_file) { return; }

  PyObject *peer, *types;
  PyObject *arglist, *result;

  peer = get_peer (C->id, (void *)C);
  types = get_update_types (flags);

  arglist = Py_BuildValue("(OO)", peer, types);
  result = PyEval_CallObject(_py_chat_update, arglist);
  Py_DECREF(arglist);
  check_result(result);
}

////extern tgl_peer_t *Peers[];
////extern int peer_num;
//
#define MAX_PY_COMMANDS 1000
void *py_ptr[MAX_PY_COMMANDS];
static int pos;
//
//static inline tgl_peer_t *get_peer (const char *s) { 
//  return tgl_peer_get_by_name (TLS, s);
//}
  
enum py_query_type {
  pq_contact_list,
  pq_dialog_list,
  pq_msg,
  pq_send_typing,
  pq_send_typing_abort,
  pq_rename_chat,
  pq_send_photo,
  pq_chat_set_photo,
  pq_set_profile_photo,
  pq_set_profile_name,
  pq_send_video,
  pq_send_text,
  pq_fwd,
  pq_fwd_media,
  pq_load_photo,
  pq_load_video_thumb,
  pq_load_video,
  pq_chat_info,
  pq_user_info,
  pq_history,
  pq_chat_add_user,
  pq_chat_del_user,
  pq_add_contact,
  pq_del_contact,
  pq_rename_contact,
  pq_search,
  pq_global_search,
  pq_mark_read,
  pq_create_secret_chat,
  pq_create_group_chat,
  pq_send_audio,
  pq_send_document,
  pq_send_file,
  pq_load_audio,
  pq_load_document,
  pq_load_document_thumb,
  pq_delete_msg,
  pq_restore_msg,
  pq_accept_secret_chat,
  pq_send_contact,
  pq_status_online,
  pq_status_offline,
  pq_send_location,
  pq_extf
};

struct py_query_extra {
  int func;
  int param;
};

void py_empty_cb (struct tgl_state *TLSR, void *cb_extra, int success) {
//  assert (TLSR == TLS);
//  struct lua_query_extra *cb = cb_extra;
//  lua_settop (luaState, 0);
//  //lua_checkstack (luaState, 20);
//  my_lua_checkstack (luaState, 20);
//
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->func);
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  lua_pushnumber (luaState, success);
//
//  assert (lua_gettop (luaState) == 3);
//
//  int r = lua_pcall (luaState, 2, 0, 0);
//
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->func);
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  if (r) {
//    logprintf ("lua: %s\n",  lua_tostring (luaState, -1));
//  }
//
//  free (cb);
}

void py_contact_list_cb (struct tgl_state *TLSR, void *cb_extra, int success, int num, struct tgl_user **UL) {
//  assert (TLSR == TLS);
//  struct lua_query_extra *cb = cb_extra;
//  lua_settop (luaState, 0);
//  //lua_checkstack (luaState, 20);
//  my_lua_checkstack (luaState, 20);
//
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->func);
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  lua_pushnumber (luaState, success);
//
//  if (success) {
//    lua_newtable (luaState);
//    int i;
//    for (i = 0; i < num; i++) {
//      lua_pushnumber (luaState, i);
//      push_peer (UL[i]->id, (void *)UL[i]);
//      lua_settable (luaState, -3);
//    }
//  } else {
//    lua_pushboolean (luaState, 0);
//  }
//
//  assert (lua_gettop (luaState) == 4);
//
//  int r = lua_pcall (luaState, 3, 0, 0);
//
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->func);
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  if (r) {
//    logprintf ("lua: %s\n",  lua_tostring (luaState, -1));
//  }
//
//  free (cb);
}

void py_dialog_list_cb (struct tgl_state *TLSR, void *cb_extra, int success, int num, tgl_peer_id_t peers[], int msgs[], int unread[]) {
//  assert (TLSR == TLS);
//  struct lua_query_extra *cb = cb_extra;
//  lua_settop (luaState, 0);
//  //lua_checkstack (luaState, 20);
//  my_lua_checkstack (luaState, 20);
//
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->func);
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  lua_pushnumber (luaState, success);
//  if (success) {
//    lua_newtable (luaState);
//    int i;
//    for (i = 0; i < num; i++) {
//      lua_pushnumber (luaState, i);
//
//      lua_newtable (luaState);
//
//      lua_pushstring (luaState, "peer");
//      push_peer (peers[i], tgl_peer_get (TLS, peers[i]));
//      lua_settable (luaState, -3);
//
//      struct tgl_message *M = tgl_message_get (TLS, msgs[i]);
//      if (M && (M->flags & FLAG_CREATED)) {
//        lua_pushstring (luaState, "message");
//        push_message (M);
//        lua_settable (luaState, -3);
//      }
//
//      lua_pushstring (luaState, "unread");
//      lua_pushnumber (luaState, unread[i]);
//      lua_settable (luaState, -3);
//
//      lua_settable (luaState, -3);
//    }
//  } else {
//    lua_pushboolean (luaState, 0);
//  }
//  assert (lua_gettop (luaState) == 4);
//
//
//  int r = lua_pcall (luaState, 3, 0, 0);
//
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->func);
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  if (r) {
//    logprintf ("lua: %s\n",  lua_tostring (luaState, -1));
//  }
//
//  free (cb);
}

void py_msg_cb (struct tgl_state *TLSR, void *cb_extra, int success, struct tgl_message *M) {
  assert (TLSR == TLS);
//  struct lua_query_extra *cb = cb_extra;
//  lua_settop (luaState, 0);
//  //lua_checkstack (luaState, 20);
//  my_lua_checkstack (luaState, 20);
//
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->func);
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  lua_pushnumber (luaState, success);
//
//  if (success) {
//    push_message (M);
//  } else {
//    lua_pushboolean (luaState, 0);
//  }
//
//  assert (lua_gettop (luaState) == 4);
//
//  int r = lua_pcall (luaState, 3, 0, 0);
//
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->func);
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  if (r) {
//    logprintf ("lua: %s\n",  lua_tostring (luaState, -1));
//  }
//
//  free (cb);
}

void py_msg_list_cb (struct tgl_state *TLSR, void *cb_extra, int success, int num, struct tgl_message *M[]) {
//  assert (TLSR == TLS);
//  struct lua_query_extra *cb = cb_extra;
//  lua_settop (luaState, 0);
//  //lua_checkstack (luaState, 20);
//  my_lua_checkstack (luaState, 20);
//
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->func);
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  lua_pushnumber (luaState, success);
//
//  if (success) {
//    lua_newtable (luaState);
//    int i;
//    for (i = 0; i < num; i++) {
//      lua_pushnumber (luaState, i);
//      push_message (M[i]);
//      lua_settable (luaState, -3);
//    }
//  } else {
//    lua_pushboolean (luaState, 0);
//  }
//
//  assert (lua_gettop (luaState) == 4);
//
//  int r = lua_pcall (luaState, 3, 0, 0);
//
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->func);
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  if (r) {
//    logprintf ("lua: %s\n",  lua_tostring (luaState, -1));
//  }
//
//  free (cb);
}

void py_file_cb (struct tgl_state *TLSR, void *cb_extra, int success, char *file_name) {
//  assert (TLSR == TLS);
//  struct lua_query_extra *cb = cb_extra;
//  lua_settop (luaState, 0);
//  //lua_checkstack (luaState, 20);
//  my_lua_checkstack (luaState, 20);
//
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->func);
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  lua_pushnumber (luaState, success);
//
//  if (success) {
//    lua_pushstring (luaState, file_name);
//  } else {
//    lua_pushboolean (luaState, 0);
//  }
//
//  assert (lua_gettop (luaState) == 4);
//
//  int r = lua_pcall (luaState, 3, 0, 0);
//
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->func);
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  if (r) {
//    logprintf ("lua: %s\n",  lua_tostring (luaState, -1));
//  }
//
//  free (cb);
}

void py_chat_cb (struct tgl_state *TLSR, void *cb_extra, int success, struct tgl_chat *C) {
//  assert (TLSR == TLS);
//  struct lua_query_extra *cb = cb_extra;
//  lua_settop (luaState, 0);
//  //lua_checkstack (luaState, 20);
//  my_lua_checkstack (luaState, 20);
//
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->func);
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  lua_pushnumber (luaState, success);
//
//  if (success) {
//    push_peer (C->id, (void *)C);
//  } else {
//    lua_pushboolean (luaState, 0);
//  }
//
//  assert (lua_gettop (luaState) == 4);
//
//  int r = lua_pcall (luaState, 3, 0, 0);
//
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->func);
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  if (r) {
//    logprintf ("lua: %s\n",  lua_tostring (luaState, -1));
//  }
//
//  free (cb);
}

void py_secret_chat_cb (struct tgl_state *TLSR, void *cb_extra, int success, struct tgl_secret_chat *C) {
//  assert (TLSR == TLS);
//  struct lua_query_extra *cb = cb_extra;
//  lua_settop (luaState, 0);
//  //lua_checkstack (luaState, 20);
//  my_lua_checkstack (luaState, 20);
//
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->func);
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  lua_pushnumber (luaState, success);
//
//  if (success) {
//    push_peer (C->id, (void *)C);
//  } else {
//    lua_pushboolean (luaState, 0);
//  }
//
//  assert (lua_gettop (luaState) == 4);
//
//  int r = lua_pcall (luaState, 3, 0, 0);
//
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->func);
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  if (r) {
//    logprintf ("lua: %s\n",  lua_tostring (luaState, -1));
//  }
//
//  free (cb);
}

void py_user_cb (struct tgl_state *TLSR, void *cb_extra, int success, struct tgl_user *C) {
//  assert (TLSR == TLS);
//  struct lua_query_extra *cb = cb_extra;
//  lua_settop (luaState, 0);
//  //lua_checkstack (luaState, 20);
//  my_lua_checkstack (luaState, 20);
//
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->func);
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  lua_pushnumber (luaState, success);
//
//  if (success) {
//    push_peer (C->id, (void *)C);
//  } else {
//    lua_pushboolean (luaState, 0);
//  }
//
//  assert (lua_gettop (luaState) == 4);
//
//  int r = lua_pcall (luaState, 3, 0, 0);
//
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->func);
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  if (r) {
//    logprintf ("lua: %s\n",  lua_tostring (luaState, -1));
//  }
//
//  free (cb);
}

void py_str_cb (struct tgl_state *TLSR, void *cb_extra, int success, char *data) {
//  assert (TLSR == TLS);
//  struct lua_query_extra *cb = cb_extra;
//  lua_settop (luaState, 0);
//  //lua_checkstack (luaState, 20);
//  my_lua_checkstack (luaState, 20);
//
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->func);
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  lua_pushnumber (luaState, success);
//
//  if (success) {
//    lua_pushstring (luaState, data);
//  } else {
//    lua_pushboolean (luaState, 0);
//  }
//
//  assert (lua_gettop (luaState) == 4);
//
//  int r = lua_pcall (luaState, 3, 0, 0);
//
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->func);
//  luaL_unref (luaState, LUA_REGISTRYINDEX, cb->param);
//
//  if (r) {
//    logprintf ("lua: %s\n",  lua_tostring (luaState, -1));
//  }
//
//  free (cb);
}

void py_do_all (void) {
  int p = 0;
  while (p < pos) {
    assert (p + 1 <= pos);

    enum py_query_type f = (long)py_ptr[p ++];
    PyObject *args = (PyObject *)py_ptr[p ++];
    char *s, *s1, *s2, *s3, *s4;
    struct tgl_message *M;
    tgl_peer_id_t peer;

    switch (f) {
    case pq_contact_list:
      tgl_do_update_contact_list (TLS, py_contact_list_cb, NULL);
      break;
    case pq_dialog_list:
      tgl_do_get_dialog_list (TLS, py_dialog_list_cb, NULL);
      break;
    case pq_msg:
      PyArg_ParseTuple(args, "iis", &peer.type, &peer.id, &s1);
      
      tgl_do_send_message (TLS, peer, s1, strlen (s1), py_msg_cb, NULL);
      break;
    case pq_send_typing:
      tgl_do_send_typing (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, tgl_typing_typing, py_empty_cb, py_ptr[p]);
      break;
    case pq_send_typing_abort:
      tgl_do_send_typing (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, tgl_typing_cancel, py_empty_cb, py_ptr[p]);
      break;
    case pq_rename_chat:
      tgl_do_rename_chat (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, py_ptr[p + 2], py_msg_cb, py_ptr[p]);
      break;
    case pq_send_photo:
      tgl_do_send_document (TLS, -1, ((tgl_peer_t *)py_ptr[p + 1])->id, py_ptr[p + 2], py_msg_cb, py_ptr[p]);
      break;
    case pq_send_video:
      tgl_do_send_document (TLS, FLAG_DOCUMENT_VIDEO, ((tgl_peer_t *)py_ptr[p + 1])->id, py_ptr[p + 2], py_msg_cb, py_ptr[p]);
      break;
    case pq_send_audio:
      tgl_do_send_document (TLS, FLAG_DOCUMENT_AUDIO, ((tgl_peer_t *)py_ptr[p + 1])->id, py_ptr[p + 2], py_msg_cb, py_ptr[p]);
      break;
    case pq_send_document:
      tgl_do_send_document (TLS, 0, ((tgl_peer_t *)py_ptr[p + 1])->id, py_ptr[p + 2], py_msg_cb, py_ptr[p]);
      break;
    case pq_send_file:
      tgl_do_send_document (TLS, -2, ((tgl_peer_t *)py_ptr[p + 1])->id, py_ptr[p + 2], py_msg_cb, py_ptr[p]);
      break;
    case pq_send_text:
      tgl_do_send_text (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, py_ptr[p + 2], py_msg_cb, py_ptr[p]);
      break;
    case pq_chat_set_photo:
      tgl_do_set_chat_photo (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, py_ptr[p + 2], py_msg_cb, py_ptr[p]);
      break;
    case pq_load_photo:
    case pq_load_video:
    case pq_load_audio:
    case pq_load_document:
      M = py_ptr[p + 1];
      if (!M || (M->media.type != tgl_message_media_photo && M->media.type != tgl_message_media_photo_encr && M->media.type != tgl_message_media_document && M->media.type != tgl_message_media_document_encr)) {
        py_file_cb (TLS, py_ptr[p], 0, 0);
      } else {
        if (M->media.type == tgl_message_media_photo) {
          tgl_do_load_photo (TLS, &M->media.photo, py_file_cb, py_ptr[p]);
        } else if (M->media.type == tgl_message_media_document) {
          tgl_do_load_document (TLS, &M->media.document, py_file_cb, py_ptr[p]);
        } else {
          tgl_do_load_encr_document (TLS, &M->media.encr_document, py_file_cb, py_ptr[p]);
        }
      }
      break;
    case pq_load_video_thumb:
    case pq_load_document_thumb:
      M = py_ptr[p + 1];
      if (!M || (M->media.type != tgl_message_media_document)) {
        py_file_cb (TLS, py_ptr[p], 0, 0);
      } else {
        tgl_do_load_document_thumb (TLS, &M->media.document, py_file_cb, py_ptr[p]);
      }
      break;
    case pq_fwd:
      tgl_do_forward_message (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, ((struct tgl_message *)py_ptr[p + 2])->id, py_msg_cb, py_ptr[p]);
      break;
    case pq_fwd_media:
      tgl_do_forward_media (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, ((struct tgl_message *)py_ptr[p + 2])->id, py_msg_cb, py_ptr[p]);
      break;
    case pq_chat_info:
      tgl_do_get_chat_info (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, 0, py_chat_cb, py_ptr[p]);
      break;
    case pq_user_info:
      tgl_do_get_user_info (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, 0, py_user_cb, py_ptr[p]);
      break;
    case pq_history:
      tgl_do_get_history (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, (long)py_ptr[p + 2], 0, py_msg_list_cb, py_ptr[p]);
      break;
    case pq_chat_add_user:
      tgl_do_add_user_to_chat (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, ((tgl_peer_t *)py_ptr[p + 2])->id, 10, py_msg_cb, py_ptr[p]);
      break;
    case pq_chat_del_user:
      tgl_do_del_user_from_chat (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, ((tgl_peer_t *)py_ptr[p + 2])->id, py_msg_cb, py_ptr[p]);
      break;
    case pq_add_contact:
      tgl_do_add_contact (TLS, s1, strlen (s1), s2, strlen (s2), s3, strlen (s3), 0, py_contact_list_cb, py_ptr[p]);
      break;
    case pq_del_contact:
      tgl_do_del_contact (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, py_empty_cb, py_ptr[p]);
      break;
    case pq_rename_contact:
      tgl_do_add_contact (TLS, s1, strlen (s1), s2, strlen (s2), s3, strlen (s3), 1, py_contact_list_cb, py_ptr[p]);
      break;
    case pq_search:
      tgl_do_msg_search (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, 0, 0, 40, 0, s, py_msg_list_cb, py_ptr[p]);
      break;
    case pq_global_search:
      tgl_do_msg_search (TLS, tgl_set_peer_id (TGL_PEER_UNKNOWN, 0), 0, 0, 40, 0, s, py_msg_list_cb, py_ptr[p]);
      break;
    case pq_mark_read:
      tgl_do_mark_read (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, py_empty_cb, py_ptr[p]);
      break;
    case pq_set_profile_photo:
      tgl_do_set_profile_photo (TLS, s, py_empty_cb, py_ptr[p]);
      break;
    case pq_set_profile_name:
      tgl_do_set_profile_name (TLS, s1, s2, py_user_cb, py_ptr[p]);
      break;
    case pq_create_secret_chat:
      tgl_do_create_secret_chat (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, py_secret_chat_cb, py_ptr[p]);
      break;
    case pq_create_group_chat:
      tgl_do_create_group_chat (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, s, py_msg_cb, py_ptr[p]);
      break;
    case pq_delete_msg:
      tgl_do_delete_msg (TLS, ((struct tgl_message *)py_ptr[p + 1])->id, py_empty_cb, py_ptr[p]);
      break;
    case pq_restore_msg:
      tgl_do_delete_msg (TLS, (long)py_ptr[p + 1], py_empty_cb, py_ptr[p]);
      break;
    case pq_accept_secret_chat:
      tgl_do_accept_encr_chat_request (TLS, py_ptr[p + 1], py_secret_chat_cb, py_ptr[p]);
      break;
    case pq_send_contact:
      tgl_do_send_contact (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id, s1, strlen (s1), s2, strlen (s2), s3, strlen (s3), py_msg_cb, py_ptr[p]);
      break;
    case pq_status_online:
      tgl_do_update_status (TLS, 1, py_empty_cb, py_ptr[p]);
      break;
    case pq_status_offline:
      tgl_do_update_status (TLS, 0, py_empty_cb, py_ptr[p]);
      break;
    case pq_extf:
      tgl_do_send_extf (TLS, s, strlen (s), py_str_cb, py_ptr[p]);
      break;
    case pq_send_location:
      if (sizeof (void *) == 4) {
        tgl_do_send_location (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id , *(float *)(py_ptr + p + 2), *(float *)(py_ptr + p + 3), py_msg_cb, py_ptr[p]);
      } else {
        tgl_do_send_location (TLS, ((tgl_peer_t *)py_ptr[p + 1])->id , *(double *)(py_ptr + p + 2), *(double *)(py_ptr + p + 3), py_msg_cb, py_ptr[p]);
      }
      break;
  /*
  pq_delete_msg,
  pq_restore_msg,
    case 0:
      tgl_do_send_message (((tgl_peer_t *)py_ptr[p])->id, py_ptr[p + 1], strlen (py_ptr[p + 1]), 0, 0);
      free (py_ptr[p + 1]);
      p += 2;
      break;
    case 1:
      tgl_do_forward_message (((tgl_peer_t *)py_ptr[p])->id, (long)py_ptr[p + 1], 0, 0);
      p += 2;
      break;
    case 2:
      tgl_do_mark_read (((tgl_peer_t *)py_ptr[p])->id, 0, 0);
      p += 1;
      break;*/
    default:
      assert (0);
    }
    Py_DECREF(args);
  }
  pos = 0;
}

PyObject* push_py_func(enum py_query_type type, PyObject *args) {
  assert(pos + 2 < MAX_PY_COMMANDS);

  py_ptr[pos ++] = (void *)(long)type;
  py_ptr[pos ++] = (void *)args;

  Py_INCREF(args);
  Py_RETURN_TRUE;
}

// Register functions to push commands on the queue
PyObject* py_contact_list(PyObject *self, PyObject *args) { return push_py_func(pq_contact_list, args); }
PyObject* py_dialog_list(PyObject *self, PyObject *args) { return push_py_func(pq_dialog_list, args); }
PyObject* py_rename_chat(PyObject *self, PyObject *args) { return push_py_func(pq_rename_chat, args); }
PyObject* py_send_msg(PyObject *self, PyObject *args) { return push_py_func(pq_msg, args); }
PyObject* py_send_typing(PyObject *self, PyObject *args) { return push_py_func(pq_send_typing, args); }
PyObject* py_send_typing_abort(PyObject *self, PyObject *args) { return push_py_func(pq_send_typing_abort, args); }
PyObject* py_send_photo(PyObject *self, PyObject *args) { return push_py_func(pq_send_photo, args); }
PyObject* py_send_video(PyObject *self, PyObject *args) { return push_py_func(pq_send_video, args); }
PyObject* py_send_audio(PyObject *self, PyObject *args) { return push_py_func(pq_send_audio, args); }
PyObject* py_send_document(PyObject *self, PyObject *args) { return push_py_func(pq_send_document, args); }
PyObject* py_send_file(PyObject *self, PyObject *args) { return push_py_func(pq_send_file, args); }
PyObject* py_send_text(PyObject *self, PyObject *args) { return push_py_func(pq_send_text, args); }
PyObject* py_chat_set_photo(PyObject *self, PyObject *args) { return push_py_func(pq_chat_set_photo, args); }
PyObject* py_load_photo(PyObject *self, PyObject *args) { return push_py_func(pq_load_photo, args); }
PyObject* py_load_video(PyObject *self, PyObject *args) { return push_py_func(pq_load_video, args); }
PyObject* py_load_video_thumb(PyObject *self, PyObject *args) { return push_py_func(pq_load_video_thumb, args); }
PyObject* py_load_audio(PyObject *self, PyObject *args) { return push_py_func(pq_load_audio, args); }
PyObject* py_load_document(PyObject *self, PyObject *args) { return push_py_func(pq_load_document, args); }
PyObject* py_load_document_thumb(PyObject *self, PyObject *args) { return push_py_func(pq_load_document_thumb, args); }
PyObject* py_fwd(PyObject *self, PyObject *args) { return push_py_func(pq_fwd, args); }
PyObject* py_fwd_media(PyObject *self, PyObject *args) { return push_py_func(pq_fwd_media, args); }
PyObject* py_chat_info(PyObject *self, PyObject *args) { return push_py_func(pq_chat_info, args); }
PyObject* py_user_info(PyObject *self, PyObject *args) { return push_py_func(pq_chat_info, args); }
PyObject* py_history(PyObject *self, PyObject *args) { return push_py_func(pq_history, args); }
PyObject* py_chat_add_user(PyObject *self, PyObject *args) { return push_py_func(pq_chat_add_user, args); }
PyObject* py_chat_del_user(PyObject *self, PyObject *args) { return push_py_func(pq_chat_del_user, args); }
PyObject* py_add_contact(PyObject *self, PyObject *args) { return push_py_func(pq_add_contact, args); }
PyObject* py_del_contact(PyObject *self, PyObject *args) { return push_py_func(pq_del_contact, args); }
PyObject* py_rename_contact(PyObject *self, PyObject *args) { return push_py_func(pq_rename_contact, args); }
PyObject* py_search(PyObject *self, PyObject *args) { return push_py_func(pq_search, args); }
PyObject* py_global_search(PyObject *self, PyObject *args) { return push_py_func(pq_global_search, args); }
PyObject* py_mark_read(PyObject *self, PyObject *args) { return push_py_func(pq_mark_read, args); }
PyObject* py_set_profile_photo(PyObject *self, PyObject *args) { return push_py_func(pq_set_profile_photo, args); }
PyObject* py_set_profile_name(PyObject *self, PyObject *args) { return push_py_func(pq_set_profile_name, args); }
PyObject* py_create_secret_chat(PyObject *self, PyObject *args) { return push_py_func(pq_create_secret_chat, args); }
PyObject* py_create_group_chat(PyObject *self, PyObject *args) { return push_py_func(pq_create_group_chat, args); }
PyObject* py_delete_msg(PyObject *self, PyObject *args) { return push_py_func(pq_delete_msg, args); }
PyObject* py_restore_msg(PyObject *self, PyObject *args) { return push_py_func(pq_restore_msg, args); }
PyObject* py_accept_secret_chat(PyObject *self, PyObject *args) { return push_py_func(pq_accept_secret_chat, args); }
PyObject* py_send_contact(PyObject *self, PyObject *args) { return push_py_func(pq_send_contact, args); }
PyObject* py_status_online(PyObject *self, PyObject *args) { return push_py_func(pq_status_online, args); }
PyObject* py_status_offline(PyObject *self, PyObject *args) { return push_py_func(pq_status_offline, args); }
PyObject* py_send_location(PyObject *self, PyObject *args) { return push_py_func(pq_send_location, args); }
PyObject* py_extf(PyObject *self, PyObject *args) { return push_py_func(pq_extf, args); }

static PyMethodDef py_tgl_methods[] = {
  {"get_contact_list", py_contact_list, METH_VARARGS, "retrieve contact list"},
  {"get_dialog_list", py_dialog_list, METH_VARARGS, ""},
  {"rename_chat", py_rename_chat, METH_VARARGS, ""},
  {"send_msg", py_send_msg, METH_VARARGS, "send message to user or chat"},
  {"send_typing", py_send_typing, METH_VARARGS, ""},
  {"send_typing_abort", py_send_typing_abort, METH_VARARGS, ""},
  {"send_photo", py_send_photo, METH_VARARGS, ""},
  {"send_video", py_send_video, METH_VARARGS, ""},
  {"send_audio", py_send_audio, METH_VARARGS, ""},
  {"send_document", py_send_document, METH_VARARGS, ""},
  {"send_file", py_send_file, METH_VARARGS, ""},
  {"send_text", py_send_text, METH_VARARGS, ""},
  {"chat_set_photo", py_chat_set_photo, METH_VARARGS, ""},
  {"load_photo", py_load_photo, METH_VARARGS, ""},
  {"load_video", py_load_video, METH_VARARGS, ""},
  {"load_video_thumb", py_load_video_thumb, METH_VARARGS, ""},
  {"load_audio", py_load_audio, METH_VARARGS, ""},
  {"load_document", py_load_document, METH_VARARGS, ""},
  {"load_document_thumb", py_load_document_thumb, METH_VARARGS, ""},
  {"fwd_msg", py_fwd, METH_VARARGS, ""},
  {"fwd_media", py_fwd_media, METH_VARARGS, ""},
  {"chat_info", py_chat_info, METH_VARARGS, ""},
  {"user_info", py_user_info, METH_VARARGS, ""},
  {"get_history", py_history, METH_VARARGS, ""},
  {"chat_add_user", py_chat_add_user, METH_VARARGS, ""},
  {"chat_del_user", py_chat_del_user, METH_VARARGS, ""},
  {"add_contact", py_add_contact, METH_VARARGS, ""},
  {"del_contact", py_del_contact, METH_VARARGS, ""},
  {"rename_contact", py_rename_contact, METH_VARARGS, ""},
  {"msg_search", py_search, METH_VARARGS, ""},
  {"msg_global_search", py_global_search, METH_VARARGS, ""},
  {"mark_read", py_mark_read, METH_VARARGS, ""},
  {"set_profile_photo", py_set_profile_photo, METH_VARARGS, ""},
  {"set_profile_name", py_set_profile_name, METH_VARARGS, ""},
  {"create_secret_chat", py_create_secret_chat, METH_VARARGS, ""},
  {"create_group_chat", py_create_group_chat, METH_VARARGS, ""},
  {"delete_msg", py_delete_msg, METH_VARARGS, ""},
  {"restore_msg", py_restore_msg, METH_VARARGS, ""},
  {"accept_secret_chat", py_accept_secret_chat, METH_VARARGS, ""},
  {"send_contact", py_send_contact, METH_VARARGS, ""},
  {"status_online", py_status_online, METH_VARARGS, ""},
  {"status_offline", py_status_offline, METH_VARARGS, ""},
  {"send_location", py_send_location, METH_VARARGS, ""},  
  {"ext_function", py_extf, METH_VARARGS, ""},
  { NULL, NULL, 0, NULL }
};


//static void lua_postpone_alarm (evutil_socket_t fd, short what, void *arg) {
//  int *t = arg;
//  
//  lua_settop (luaState, 0);
//  //lua_checkstack (luaState, 20);
//  my_lua_checkstack (luaState, 20);
//
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, t[1]);
//  lua_rawgeti (luaState, LUA_REGISTRYINDEX, t[0]);
//  assert (lua_gettop (luaState) == 2);
//  
//  int r = lua_pcall (luaState, 1, 0, 0);
//
//  luaL_unref (luaState, LUA_REGISTRYINDEX, t[0]);
//  luaL_unref (luaState, LUA_REGISTRYINDEX, t[1]);
//
//  if (r) {
//    logprintf ("lua: %s\n",  lua_tostring (luaState, -1));
//  }
//
//}
//
//static int postpone_from_lua (lua_State *L) {
//  int n = lua_gettop (L);
//  if (n != 3) {
//    lua_pushboolean (L, 0);
//    return 1;
//  }
//
//  double timeout = lua_tonumber (L, -1);
//  if (timeout < 0) {
//    lua_pushboolean (L, 0);
//    return 1;
//  }
//
//  lua_pop (L, 1);
//  int a1 = luaL_ref (L, LUA_REGISTRYINDEX);
//  int a2 = luaL_ref (L, LUA_REGISTRYINDEX);
//
//
//  int *t = malloc (16);
//  assert (t);
//  struct event *ev = evtimer_new (TLS->ev_base, lua_postpone_alarm, t);
//  t[0] = a1;
//  t[1] = a2;
//  *(void **)(t + 2) = ev;
//  
//  struct timeval ts= {
//    .tv_sec = (long)timeout,
//    .tv_usec = (timeout - ((long)timeout)) * 1000000
//  };
//  event_add (ev, &ts);
//  
//  lua_pushboolean (L, 1);
//  return 1;
//}

//extern int safe_quit;
//static int safe_quit_from_lua (lua_State *L) {
//  int n = lua_gettop (L);
//  if (n != 0) {
//    lua_pushboolean (L, 0);
//    return 1;
//  }
//  safe_quit = 1;
//  
//  lua_pushboolean (L, 1);
//  return 1;
//}
//

#define my_python_register(dict, name, f) \
    f = PyDict_GetItemString(dict, name); 


void inittgl()
{
  (void) Py_InitModule("tgl", py_tgl_methods);    
}

void py_init (const char *file) {
  if (!file) { return; }

  PyObject *pName, *pModule, *pDict;
  
  Py_Initialize();
  inittgl();

  PyObject* sysPath = PySys_GetObject((char*)"path");
  char file_path[1024];
  strncpy(file_path, file, sizeof(file_path));
  PyList_Append(sysPath, PyString_FromString(dirname(file_path)));
  // remove .py extension from file, if any
  strncpy(file_path, file, sizeof(file_path));
  char* dot = strrchr(file_path, '.');
  if (dot && strcmp(dot, ".py") == 0) 
      *dot = 0;
  printf("file_path = %s\n", file_path);
  printf("basename(file_path) = %s\n", basename(file_path));
  pModule = PyImport_Import(PyString_FromString(basename(file_path)));
  if (pModule == NULL) {
    PyErr_Print();
    Py_Finalize();
    return;
  }

  have_file = 1;

  pDict = PyModule_GetDict(pModule);

  // Store callables for python functions
  my_python_register(pDict, "on_binlog_replay_end", _py_binlog_end);
  my_python_register(pDict, "on_get_difference_end", _py_diff_end);
  my_python_register(pDict, "on_our_id", _py_our_id);
  my_python_register(pDict, "on_msg_receive", _py_new_msg);
  my_python_register(pDict, "on_secret_chat_update", _py_secret_chat_update);
  my_python_register(pDict, "on_user_update", _py_user_update);
  my_python_register(pDict, "on_chat_update", _py_chat_update);
}

