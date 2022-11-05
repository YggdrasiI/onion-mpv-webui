# onion-mpv-webui
...is a web based user interface with controls for the [mpv mediaplayer](https://mpv.io/).
The code had started as fork of [simple-mpv-webui](https://github.com/open-dynaMIX/simple-mpv-webui/),
but all LUA code was replaced by C code.

## Building
```
cd [cloned dir]
mkdir build
cmake ..
make
```

## Usage

### Enable plugin
Add `include="~~/script-opts/libwebui.conf"` to `mpv.conf` or 
call `make config` to copy libwebui.conf.example into `$HOME/.mpv/config/` directory.

Call mpv with new profile to enable plugin, e.g. `mpv --profile=webui`


You can use the webui by accessing [http://127.0.0.1:9000](http://127.0.0.1:9000) or
[http://[::1]:9000](http://[::1]:9000) in your webbrowser.

By default it listens on `0.0.0.0:9000` and `[::0]:9000`. As described below, this can be changed.

### Options
Options can be set with [--script-opts](https://mpv.io/manual/master/#options-script-opts)
with the prefix `libwebui-`.

#### port (int)
Set the port to serve the webui (default: 9000). Setting this allows for
running multiple instances on different ports.

Example:
```
mpv --profile=webui --script-opts=libwebui-port=9000
```

Alternativly, you can add the option to libwebui.conf, e.g.
```
[…]
script-opts-add=libwebui-port=9000
script-opts-add=libwebui-hostname=::
```


## Dependencies
 - [onion](https://github.com/davidmoreno/onion)

## Screenshots
TODO


## Key bindings
There are some keybindings available:

| Key        | Function            |
| ---------- | ------------------- |
| SPACE      | Play/Pause          |
| ArrowRight | Seek +10            |
| ArrowLeft  | Seek -10            |
| PageDown   | Seek +3             |
| PageUp     | Seek -3             |
| f          | Toggle fullscreen   |
| n          | Playlist next       |
| p          | Playlist previous   |
| BACKSPACE  | Reset playback speed|

## Media Session API
When using a browser that supports it, onion-mpv-webui uses the Media Session
API to provide a notification with some metadata and controls:

In order to have the notification work properly you need to at least once trigger play from the webui.

## Endpoints
You can also directly talk to the endpoints:
(TODO: Incomplete list)

| URI                                    | Method | Parameter                          | Description                                                             |
| -------------------------------------- | ------ | ---------------------------------- | ----------------------------------------------------------------------- |
| /api/status                            | GET    |                                    | Returns JSON data about playing media --> see below                     |
| /api/play                              | BOTH   |                                    | Play media                                                              |
| /api/pause                             | BOTH   |                                    | Pause media                                                             |
| /api/toggle_pause                      | BOTH   |                                    | Toggle play/pause                                                       |
| /api/fullscreen                        | BOTH   |                                    | Toggle fullscreen                                                       |
| /api/seek/:seconds                     | POST   | `int` or `float` (can be negative) | Seek                                                                    |
| /api/set_position/:seconds             | POST   |                                    | Go to position :seconds                                                 |
| /api/playlist_prev                     | POST   |                                    | Go to previous media in playlist                                        |
| /api/playlist_next                     | POST   |                                    | Go to next media in playlist                                            |
| /api/playlist_jump/:index              | POST   | `int`                              | Jump to playlist item at position `:index`                              |
| /api/playlist_move/:source/:target     | POST   | `int` and `int`                    | Move playlist item from position `:source` to position `:target`        |
| /api/playlist_move_up/:index           | POST   | `int`                              | Move playlist item at position `:index` one position up                 |
| /api/playlist_remove/:index            | POST   | `int`                              | Remove playlist item at position `:index`                               |
| /api/playlist_shuffle                  | POST   |                                    | Shuffle the playlist                                                    |
| /api/loop_file/:mode                   | POST   | `string` or `int`                  | Loop the current file. `:mode` accepts the same loop modes as mpv      |
| /api/loop_playlisr/:mode               | POST   | `string` or `int`                  | Loop the whole playlist `:mode` accepts the same loop modes as mpv     |
| /api/add_chapter/:amount               | POST   | `int` (can be negative)            | Jump `:amount` chapters in current media                                |
| /api/add_volume/:percent               | POST   | `int` or `float` (can be negative) | Add :percent% volume                                                    |
| /api/set_volume/:percent               | POST   | `int` or `float`                   | Set volume to :percent%                                                 |
| /api/add_sub_delay/:ms                 | POST   | `int` or `float` (can be negative) | Add :ms milliseconds subtitles delay                                    |
| /api/set_sub_delay/:ms                 | POST   | `int` or `float` (can be negative) | Set subtitles delay to :ms milliseconds                                 |
| /api/add_audio_delay/:ms               | POST   | `int` or `float` (can be negative) | Add :ms miliseconds audio delay                                         |
| /api/set_audio_delay/:ms               | POST   | `int` or `float` (can be negative) | Set audio delay to :ms milliseconds                                     |
| /api/cycle_sub                         | POST   |                                    | Cycle trough available subtitles                                        |
| /api/cycle_audio                       | POST   |                                    | Cycle trough available audio tracks                                     |
| /api/cycle_audio_device                | POST   |                                    | Cycle trough audio devices. [More information.](#audio-devices-string)  |
| New in this fork                       |        |                                    |                                                                         |
| /media/api/list/:share_name/:sub_dir   | GET    | `string` and `string`              | Returns JSON data about shared folder                                   |
| /media/api/html/:share_name/:sub_dir   | GET    | `string` and `string`              | Directory listing of shared folders, if defined.                        |
| /ws                                    | GET    |                                    | Websocket test page                                                     |

All POST endpoints return a JSON message. If successful: `{"message": "success"}`, otherwise, the message will contain
information about the error.

### /api/status
`metadata` contains all the metadata mpv can see, below is just an example:

``` json
{
    "filename": "big_buck_bunny_1080p_stereo.ogg",
    "chapter": 0,            # <-- current chapter
    "chapters": 0,           # <-- chapters count
    "duration": 596,         # <-- seconds
    "position": 122,         # <-- seconds
    "pause": true,
    "remaining": 474,        # <-- seconds
    "sub-delay": 0,          # <-- milliseconds
    "audio-delay": 0,        # <-- milliseconds
    "fullscreen": false,
    "loop-file": "no",       # <-- `no`, `inf` or integer
    "loop-playlist": "no",   # <-- `no`, `inf`, `force` or integer
    "metadata": {},          # <-- All metadata available to MPV
    "track-list": [          # <-- All available video, audio and sub tracks
        {
            "id": 1,
            "type": "video",
            "src-id": 0,
            "albumart": false,
            "default": false,
            "forced": false,
            "external": false,
            "selected": true,
            "ff-index": 0,
            "decoder-desc": "theora (Theora)",
            "codec": "theora",
            "demux-w": 1920,
            "demux-h": 1080
        },
        {
            "id": 1,
            "type": "audio",
            "src-id": 0,
            "audio-channels": 2,
            "albumart": false,
            "default": false,
            "forced": false,
            "external": false,
            "selected": true,
            "ff-index": 1,
            "decoder-desc": "vorbis (Vorbis)",
            "codec": "vorbis",
            "demux-channel-count": 2,
            "demux-channels": "stereo",
            "demux-samplerate": 48000
        }
    ],
    "volume": 64,
    "volume-max": 130,
    "playlist": [            # <-- All files in the current playlist
        {
            "filename": "Videos/big_buck_bunny_1080p_stereo.ogg",
            "current": true,
            "playing": true
        }
    ]
    , […]
}
```

## Differences to simple-mpv-webui
 - C-Plugin
 - Websocket protocol used for status updates
 - Added shared_folder option. Allows adding local files to playlist over GUI
 - Better button layout on slim screens.

