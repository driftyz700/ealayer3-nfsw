# ealayer3-nfsw
Updated with instructions on how to use in Need For Speed World (Work in progress, some stuff may not be fully accurate)

So how do you get custom music to work with nfsw?

NFS World Supports: ealayer3 codec version 6 headerless with its header located in the .snr file and the audio contents in .sns

Suprisingly this tool can only encode music in a previous ealayer3 codec version 5 yet nfs world can play it just fine, interesting..

[HOW TO] Part 1 (Creating the custom audio tracks):
1) Open cmd
2) 'cd 'directory\of\ealayer3\tool' - example: "C:\Users\DriFtyZ\Downloads\ealayer3-nfsw"
3) 'ealayer3.exe -E [song-name].mp3 --two-files' - Will produce 2 files (non looped) ending with .ealayer3 and .ealayer3.header, simply rename them to .sns and .snr perspectively for nfsw 
- if you want looped music files simply add "--loop" argument in your command and it should produce looping music files 
- although the tool doesn't know what to write in last 8 bytes (08-15) of the snr header which i think they have to do with specifying at which time inside the audio track the second part will start playing.
- tool also supports multi channels by adding more music file names after -E argument example: "ealayer3.exe -E song.mp3 song_2.mp3 --two-files --loop"
-though if you encode looping sounds with multiple channels they will all start playing at the same time in playback within the game so avoid using it.
4) Place both newly created .snr and .sns files into "NFSW Directory/Sound/Music"

----------------------------------------------------------------------------

[HOW TO] Part 2 (Adding the custom audio tracks into the game database)

Now how do you import the custom music to NFS World?

NFS World primarily uses 3 vlt vaults for storing music data

1) blackboard (Specifies which tracks will play at certain "moments" and their settings (such as how many tracks, volume, and other effects)
2) audiosystem (Contains the paths of all audio banks in the game as well as defines the usable "rwac_music" nodes responsible for defining our custom music files)
3) rwac_music (Contains the music nodes at which each one contains a link the appropriate .snr file. It gets referenced by blackboard/audio_* and audiosystem/mostwanted nodes)

Let's say now we want to change the iconic dual freeroam_04_loopable and hero tracks during free roam to have a variety of more custom audio tracks

We want to add 3 custom tracks to be played at random order everytime you enter freeroam, while disabling the original audio tracks
The custom audio tracks that we created using the ealayer3 tool are named "CustomTrack_01.snr/sns | CustomTrack_02.snr/sns | CustomTrack_03.snr/sns" 

1) Open VltEd.exe (if you don't have it a quick google search will find you what you need)
2) Look for the "rwac_music" vault
it will contain "default" node with all child nodes for the music used in the game
- Add your own custom nodes under "default" for each track we want to add (3 nodes named customtrack_01, customtrack_02 and customtrack_03)
- Add "Optional" Field named 0xdaa0c91a to each newly create node and fill it with custom track filename perspectively
3) Look for the audiosystem/mostwanted node, you will see an array list named 0xab458147, it will have a size of 17 entries
- Change size of array 0xab458147 from 17 to 20 since we're going to add 3 custom audio tracks
- Fill each new array class with "rwac_music" value and collection with our own previously created rwac_music nodes that will be used the custom tracks
4) Look for the blackboard/audio/audio_freeroam node 
- resize the array of "EAMusicList" to the number of track we want to include in the freeroam music pool (3 in our case)
- define your custom music rwac_music nodes we created before
- disable the hero music interrupting the main one

For extra help, the following is a complete nfs modscript example of what we are trying to achieve
<pre>
#DEFINE OUR CUSTOM MUSIC RWAC_MUSIC NODES
resize_field audiosystem mostwanted 0xab458147 20
update_field audiosystem mostwanted 0xab458147[17] Class rwac_music
update_field audiosystem mostwanted 0xab458147[18] Class rwac_music
update_field audiosystem mostwanted 0xab458147[19] Class rwac_music
update_field audiosystem mostwanted 0xab458147[17] Collection customtrack_01
update_field audiosystem mostwanted 0xab458147[18] Collection customtrack_02
update_field audiosystem mostwanted 0xab458147[19] Collection customtrack_03

#CREATE OUR CUSTOM MUSIC RWAC_MUSIC NODES
add_node rwac_music default customtrack_01
add_node rwac_music default customtrack_02
add_node rwac_music default customtrack_03
add_field rwac_music customtrack_01 0xdaa0c91a
add_field rwac_music customtrack_02 0xdaa0c91a
add_field rwac_music customtrack_03 0xdaa0c91a
update_field rwac_music customtrack_01 0xdaa0c91a CustomTrack_01.snr
update_field rwac_music customtrack_02 0xdaa0c91a CustomTrack_02.snr
update_field rwac_music customtrack_03 0xdaa0c91a CustomTrack_03.snr

#CANCEL FREEROAM HERO (We don't want this to conflict the previous track that will be most likely still playing)
update_field blackboard audio_freeroam MinTimePlayerStoppedHeroTrack 0
delete_field blackboard audio_freeroam FreeroamHeroMusicTrack
delete_field blackboard audio_freeroam MinTimePlayerAtHighSpeed
delete_field blackboard audio_freeroam TimePlayerStoppedHeroTrackThreshold

#UPDATE FREEROAM MUSIC LIST
resize_field blackboard audio_freeroam EAMusicList 3
update_field blackboard audio_freeroam EAMusicList[1] Class rwac_music
update_field blackboard audio_freeroam EAMusicList[2] Class rwac_music
update_field blackboard audio_freeroam EAMusicList[0] Collection customtrack_01
update_field blackboard audio_freeroam EAMusicList[1] Collection customtrack_02
update_field blackboard audio_freeroam EAMusicList[2] Collection customtrack_03
</pre>
CTRL + S to Save Changes and that's it! You have officially now added 3 custom tracks to nfs world freeroam music player pool

----------------------------------------------------------------------------
Now onto the more "technical" stuff  (for researchers)

let's take an example produced music file named GHOST (non looped)

opening GHOST.snr in a hex editor will show the following:

Offset (0)

05 04 AC 44 40 97 6F AF

<pre>
05:        Indicates ealayer3 codec version (5 in our case) 
04 AC 44:  Ealayer3 codec settings that don't differ between any of the audio tracks (Unknown) 
40:        Indicates whether the sound is looped or non looped (40 non looping, 60 for looping) 
97 6F AF:  Another unfigured ealayer3 header values that i think the have to do with the length of the track and the number of uncompressed samples within the track
</pre>
Rest 8 bits (08-15): Have to do with specifying at which time/point inside the track the next part will start playing \
(for looping songs like the vanilla freeroam_04_loopable.snr track, though tool doesn't support that so it'll write 0s in the bytes)


Fun fact: Based on that you can also make this to work with older versions of ealayer3 like 0.6.2 and below, though you're going to have to split the header of the sns file onto the snr manually using a hex editor

----------------------------------------------------------------------------

Have fun playing with your own custom music in game :)

Big Thanks to: \
Translu:    For patching ealayer3 to support looped files as well as proper multi-channel support, for real thanks mate \
DriFtyZ:	  For writing this doc.\
Hypercycle: For showing me that the rwac_music nodes aren't actually hardcoded and you can add more tracks to the game (based on some previous assumptions)
