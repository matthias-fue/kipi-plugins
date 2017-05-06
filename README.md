# kipi-plugins "of old"

This project is a fork of https://github.com/KDE/kipi-plugins (the KDE kipi-plugins)
that brings back some of the plugins which have been removed from KDE's kipi-plugins.

Digikam, the maintainer of kipi-plugins, has moved a lot of functionality from
kipi-plugins inside digikam itself, leaving other applications that can utilize
kipi-plugins stripped of these features.

In this project, I revert the removal of some plugins and try to make them work with
the current KDE 5.

# Scope

I'm most interested in these plugins:
 * Geo Locator
 * Adjust Date/Time
 * View / Edit Metadata
 
 and these host applications:
 * KPhotoAlbum
 * Gwenview

# Current Status

## Revived Plugins

 * Geo Locator
 
## Known Limitations and Shortcomings

 * KPMetadataProcessor not used
 
   The plugins just modify EXIF data in the files itself. If the host application provides
   a metadata processor (neither KPhotoAlbum nor Gwenview do), it is just ignored. The host
   application is not notified of the changed files -- in KPhotoAlbum, you have to trigger
   the update of the EXIF database and Recalculation of the MD5 checksums yourself. Settings
   in the host application whether changes to image files are allowed and/or a sidecar file
   should be updated/created are ignored.
   
 * Geo Locator requires "hasDate" which Gwenview doesn't provide
  
   That's why the plugin currently isn't offered there