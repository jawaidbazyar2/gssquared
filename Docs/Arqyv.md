# Arqyv Integration

[arQyv](https://arqyv.net) is a library of Apple II titles. From your Collection there, **Play** opens that title in desktop GSSquared. When cloud storage is on for your account, you can save the title back to your Collection.

The file arQyv sends is a [GS2 pack](Gs2Pack.md): a machine configuration and the disk images it uses. This page is how you play and save one. The desktop app and the browser build at [gssquared.net/live](https://gssquared.net/live) both do this. The browser copy lives only in that tab: reload or crash drops unsaved changes.

## Play a title

1. Sign in on arQyv and open **Collection**.
2. Click **Play** on the title. The title name opens the catalog page. **Play** is what opens GSSquared. On the web, Play opens gssquared.net/live with that title.
3. The desktop browser asks to open GSSquared. The web page asks you to confirm the download. Either prompt names the site and the pack, not the sign-in token.
4. Confirm it.
5. The machine boots with that title’s disks. System Select is skipped.

The desktop app keeps the downloaded pack on this computer and updates that copy when you close the machine. The web build keeps it in the tab’s memory.

## Save to Collection

**File → Save to Collection** is on while a Collection title that can be saved is running. arQyv includes that permission with the download when cloud storage is on. The item stays gray when:

- no machine is running
- the machine was not opened from a Collection **Play** link
- cloud storage is off, so the download had nothing to write back to

Choosing it stores your disks, and on an Apple IIgs the Control Panel battery settings, then uploads the pack. The window shows **Saving to arQyv...** while that upload runs. The machine keeps running.

The same upload runs when you leave the title (power off, or launch another machine) and when you quit. Quit waits until the upload finishes. If nothing in the pack changed, GSSquared does not upload. On the web, closing the tab asks whether to save to your Collection or close without saving. The page cannot close the tab for you. After a successful save it tells you the tab can be closed.

A disk you open from this computer is not part of the Collection copy. Writes to it stay in that file. See [Storage & Disks](Storage.md).

### If the Collection copy is not updated

GSSquared still updates the pack on this computer. The message says what happened to the Collection copy:

- **The Collection copy was not updated**, with a reason under it. The upload did not finish. Use **Save to Collection** again, or quit, to retry. Do that before you **Play** the title again. **Play** downloads the Collection copy, and that download replaces the pack on this computer.
- **Play the title again to save to arQyv.** The permission to save has expired. **Play** from Collection starts a new download. That download is the Collection copy, which does not include the save that was just rejected.

## What this does not do

- A public catalog title, with cloud storage off, launches as a pack and does not enable **Save to Collection**.
- GSSquared does not list your Collection. You start from **Play** on the website.
