# gracenote-cli

GNSDK-based command line tool that identifies artist, album and track name in a .wav file.

## What do I need?

* GNSDK
* Gracenote's Developer account with credentials

Just follow [the link](https://developer.gracenote.com/) and get it there.

## What to do?

* Copy `main.c` to `gnsdk-3.02.0.422o-20130108/samples/misucid_stream`
* Run `make`
* Use `./sample 'CLIENT ID' 'CLIENT TAG' ./license.txt ./data/05-Hummingbird-sample.wav`

If everything is fine, you should see the followuing output:

```bash
{ 
 'artist': 'Wilco',
 'album': 'A Ghost is Born',
 'title': 'Hummingbird'
}
```
