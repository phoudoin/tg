import tgl


our_id = 0

def on_binlog_replay_end():
    return "on_binlog_replay_end"

def on_get_difference_end():
    return "on_get_difference_end"

def on_our_id(id):
    our_id = id
    return "Set ID: " + str(our_id) 

def on_msg_receive(msg):
    print msg;
    if msg["out"]:
      return;

    if msg["service"]:
        return;

    peer = msg["from"]
    if msg["to"]["type_str"] == "chat":
        peer = msg["to"]

    if "marvin" in msg["text"]:
        tgl.send_msg(peer["type"], peer["id"], "PONG!")
    return "on_msg_receive"

def on_secret_chat_update(peer, types):
    return "on_secret_chat_update"

def on_user_update(user, types):
    return "on_user_update"

def on_chat_update(peer, types):
    return "on_chat_update"

