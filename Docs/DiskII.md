

## Writing


Let's see how other emus handle writing to floppy images. Specifically, 5.25 images, since 3.5 images are handled as just block devices.

First, we need to handle the write-to-disk register. Second, we need to de-nibblize the data and write back to disk.

First, OE. Copy DOS33 system master. Make a basic program and save it. Has it already modified the disk image? No, it has not. in Devices, there's an eject button next to the image name. Ah ha. When we 'eject' it, that is when it writes the data back to the disk image.

Virtual II. When it has the file open, it locks it which prevents other apps (such as cp2) from accessing it. when you click on the disk icon (if mounted already), it unmounts the image. That is when it writes the data back to the disk image.

ok, firstly, I think the user should be able to choose whether to write back to the disk image or not. Maybe you really don't care about the data. Also, if something went kablooey maybe you don't want to. 

Here are cases where we need to decide:
* exit emulator
* eject disk

Only manipulating the disk image in memory via the registers will be pretty easy for testing. Will then want some way (a F key perhaps) to trigger saving the disk image to a test file which can then be inspected by AppleSauce/cp2 for testing.

$C08F,X is "write". 
$C08D,X is "load data register from data bus", i.e., writing.

A great way to test this at first is going to be to format a disk. Should get a usable blank image out of it.

```
LDA DATA1
STA $C08F,X
CMP $C08C,X   ; switches SHIFT/LOAD to SHIFT.
```

"the above example illustrates the principle of writing continuous bytes of data: initiate the WRITE sequence, then store data every 32 cycles."
"After storing the last byte to be written, wait 32 cycles, switching READ/WRITE to READ on the 32nd cycle. You can store data in any multiple of 4 cycles and the data register will accept it." <-- implies we stay in write mode until it's turned off by hitting READ. 
"RWTS writes read syncing leaders by storing $FF to the data register in 40 cycle loops in DOS3.3. This creates a series of 1111111100 strings..."

For our purposes, it is likely sufficient to write the byte stored into $C08F,X once $C08C,X is referenced.

DOS33 is returning write protect error. WRITE16 returns carry set if write protect error. Carry clear if no error. So carry is being set. I need to do a diagnostic dump of this code executing. Whee!

oops:
 | PC: $B833, A: $EE, X: $60, Y: $00, P: $A5, S: $DE || B833: LDA $C08E,X   [#EE] <- $C0EE
 | PC: $B836, A: $EE, X: $60, Y: $00, P: $A5, S: $DE || B836: BMI #$7C => $B8B4 (taken)
 | PC: $B8B4, A: $EE, X: $60, Y: $00, P: $A5, S: $DE || B8B4: LDA $C08C,X   [#01] <- $C0EC

I guess we need to return something legit here.

that's an improvement, now we got an I/O error. ha ha. Let

Apparently you can write data to both Q7H and Q6H. Hm.
Write to Q7H sets write mode, and loads the data register. Q6H just loads the data register.
What is difference between "Shift while writing" and "load while writing"? And Write?
ha! It works!!!! let's so some more thorough testing..

* 2000<F800.FFFFM
* bsave test.bin,a$2000,l$800
* bload test.bin,a$3000
* F800<3000.37FFV

SUCCESS!

next step: dump the disk image to a file in nibblized format, to validate with cp2 and AppleSauce.

## De-nybblizing

Now that we support writing to a nibble image in RAM, in order to save that data back out, we need to denybblize and store back in the same sector order as before. 

How to handle each media type:
* pre-nibblized: simply write it back out. Trivial. So start there?
* .do, .po, .dsk: read data from each track. Grab each sector header. Write 256 byte chunk after seeking to (track * 16 + sector) * 256 in the file.

Write this initially as a denibblizer utility.

## Write protection (Complete)

Most operating systems allow a file to be specified as "read only" even for the current user. This seems like the best way to support passing through write-protect status to the emulator. I.e., if the OS file is read-only, then set WP flag in the media descriptor. (Implemented now in media, checked by DiskII module)

## File locking

VirtualII locks a disk image file when it's being used, preventing other software (e.g., other emulators) from accessing it at the same time.

This seems like a really, really good idea.

## Disk-drive sound emulation

Well, everyone else does it. I guess I have to, ha ha. OE has all the classic Shugart sounds.
Works, but could use some tidying up.



## Prep work for IWM , refactoring DiskII

DiskII needs to be split into routines to manage 5.25" drive stuff that are independent of the DiskII legacy code itself.

```
class FloppyDrive  {

    int track_num;
    int side;

    head_move();

    virtual mount_image();
    virtual unmount();
    virtual writeback_image();
    virtual nibblize();
    virtual status();
    virtual reset();
}

class Floppy525: FloppyDrive {
    trackdata[6656*35]

    void motor(bool);
    void track(int);

    write_nybble();
    read_nybble();

    virtual mount_image();
    virtual unmount();
    virtual writeback_image() override;
    virtual nibblize() override;
    virtual status() override;
    virtual reset() override;
}

class FloppyController {
    Floppy525 drives[2];
    FloppyController();
    ~FloppyController();

    read();
    write();
    status();
    reset();
    soundeffects
}

init_diskii {
    allocate a Floppycontroller;
    init it;
}
```

[ ] return floating bus on reads instead of 0xEE  
[ ] use timer to schedule actual motor off; right now it's not turning off in a few cases where it ought because there is no followup r/w after the motor off.  

Some of the softswitches are controller level. Some are drive level. 

