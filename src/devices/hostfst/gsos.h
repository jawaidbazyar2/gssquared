#define readEnableAllowWrite 0x0000
#define readEnable 0x0001
#define writeEnable 0x0002
#define readWriteEnable 0x0003
#define fileInvisible 0x0004            /* Invisible bit */
#define backupNeeded 0x0020             /* backup needed bit: CreateRec/ OpenRec access field. (Must be 0 in requestAccess field ) */
#define renameEnable 0x0040             /* rename enable bit: CreateRec/ OpenRec access and requestAccess fields */
#define destroyEnable 0x0080            /* destroy enable bit: CreateRec/ OpenRec access and requestAccess fields */
#define startPlus 0x0000                /* base -> setMark = displacement */
#define eofMinus 0x0001                 /* base -> setMark = eof - displacement */
#define markPlus 0x0002                 /* base -> setMark = mark + displacement */
#define markMinus 0x0003                /* base -> setMark = mark - displacement */

/* cachePriority Codes */
#define cacheOff 0x0000                 /* do not cache blocks invloved in this read */
#define cacheOn 0x0001                  /* cache blocks invloved in this read if possible */

/* Error Codes */
#define badSystemCall 0x0001            /* bad system call number */
#define invalidPcount 0x0004            /* invalid parameter count */
#define gsosActive 0x0007               /* GS/OS already active */

#ifndef devNotFound                     /* device not found */
 #define devNotFound 0x0010
#endif

#define invalidDevNum 0x0011            /* invalid device number */
#define drvrBadReq 0x0020               /* bad request or command */
#define drvrBadCode 0x0021              /* bad control or status code */
#define drvrBadParm 0x0022              /* bad call parameter */
#define drvrNotOpen 0x0023              /* character device not open */
#define drvrPriorOpen 0x0024            /* character device already open */
#define irqTableFull 0x0025             /* interrupt table full */
#define drvrNoResrc 0x0026              /* resources not available */
#define drvrIOError 0x0027              /* I/O error */
#define drvrNoDevice 0x0028             /* device not connected */
#define drvrBusy 0x0029                 /* call aborted; driver is busy */
#define drvrWrtProt 0x002B              /* device is write protected */
#define drvrBadCount 0x002C             /* invalid byte count */
#define drvrBadBlock 0x002D             /* invalid block address */
#define drvrDiskSwitch 0x002E           /* disk has been switched */
#define drvrOffLine 0x002F              /* device off line/ no media present */
#define badPathSyntax 0x0040            /* invalid pathname syntax */
#define tooManyFilesOpen 0x0042         /* too many files open on server volume */
#define invalidRefNum 0x0043            /* invalid reference number */

#ifndef pathNotFound                    /* subdirectory does not exist */
 #define pathNotFound 0x0044
#endif

#define volNotFound 0x0045              /* volume not found */

#ifndef fileNotFound                    /* file not found */
 #define fileNotFound 0x0046
#endif

#define dupPathname 0x0047              /* create or rename with existing name */
#define volumeFull 0x0048               /* volume full error */
#define volDirFull 0x0049               /* volume directory full */
#define badFileFormat 0x004A            /* version error (incompatible file format) */

#ifndef badStoreType                    /* unsupported (or incorrect) storage type */
 #define badStoreType 0x004B
#endif

#ifndef eofEncountered                  /* end-of-file encountered */
 #define eofEncountered 0x004C
#endif

#define outOfRange 0x004D               /* position out of range */
#define invalidAccess 0x004E            /* access not allowed */
#define buffTooSmall 0x004F             /* buffer too small */
#define fileBusy 0x0050                 /* file is already open */
#define dirError 0x0051                 /* directory error */
#define unknownVol 0x0052               /* unknown volume type */

#ifndef paramRangeErr                   /* parameter out of range */
 #define paramRangeErr 0x0053
#endif

#define outOfMem 0x0054                 /* out of memory */
#define dupVolume 0x0057                /* duplicate volume name */
#define notBlockDev 0x0058              /* not a block device */

#ifndef invalidLevel                    /* specifield level outside legal range */
 #define invalidLevel 0x0059
#endif

#define damagedBitMap 0x005A            /* block number too large */
#define badPathNames 0x005B             /* invalid pathnames for ChangePath */
#define notSystemFile 0x005C            /* not an executable file */
#define osUnsupported 0x005D            /* Operating System not supported */

#ifndef stackOverflow                   /* too many applications on stack */
 #define stackOverflow 0x005F
#endif

#define dataUnavail 0x0060              /* Data unavailable */
#define endOfDir 0x0061                 /* end of directory has been reached */
#define invalidClass 0x0062             /* invalid FST call class */
#define resForkNotFound 0x0063          /* file does not contain required resource */
#define invalidFSTID 0x0064             /* error - FST ID is invalid */
#define invalidFSTop 0x0065             /* invalid FST operation */
#define fstCaution 0x0066               /* FST handled call, but result is weird */
#define devNameErr 0x0067               /* device exists with same name as replacement name */
#define defListFull 0x0068              /* device list is full */
#define supListFull 0x0069              /* supervisor list is full */
#define fstError 0x006a                 /* generic FST error */
#define resExistsErr 0x0070             /* cannot expand file, resource already exists */
#define resAddErr 0x0071                /* cannot add resource fork to this type file */
#define networkError 0x0088             /* generic network error */

/* fileSys IDs */
#define proDOSFSID 0x0001               /* ProDOS/SOS */
#define dos33FSID 0x0002                /* DOS 3.3 */
#define dos32FSID 0x0003                /* DOS 3.2 */
#define dos31FSID 0x0003                /* DOS 3.1 */
#define appleIIPascalFSID 0x0004        /* Apple II Pascal */
#define mfsFSID 0x0005                  /* Macintosh (flat file system) */
#define hfsFSID 0x0006                  /* Macintosh (hierarchical file system) */
#define lisaFSID 0x0007                 /* Lisa file system */
#define appleCPMFSID 0x0008             /* Apple CP/M */
#define charFSTFSID 0x0009              /* Character FST */
#define msDOSFSID 0x000A                /* MS/DOS */
#define highSierraFSID 0x000B           /* High Sierra */
#define iso9660FSID 0x000C              /* ISO 9660 */
#define appleShareFSID 0x000D           /* ISO 9660 */

/* FSTInfo.attributes Codes */
#define characterFST 0x4000             /* character FST */
#define ucFST 0x8000                    /* SCM should upper case pathnames before passing them to the FST */

/* QuitRec.flags Codes */
#define onStack 0x8000                  /* place state information about quitting program on the quit return stack */
#define restartable 0x4000              /* the quitting program is capable of being restarted from its dormant memory */

/* storageType Codes */
#define seedling 0x0001                 /* standard file with seedling structure */
#define standardFile 0x0001             /* standard file type (no resource fork) */
#define sapling 0x0002                  /* standard file with sapling structure */
#define tree 0x0003                     /* standard file with tree structure */
#define pascalRegion 0x0004             /* UCSD Pascal region on a partitioned disk */
#define extendedFile 0x0005             /* extended file type (with resource fork) */
#define directoryFile 0x000D            /* volume directory or subdirectory file */

/* version Codes */
#define minorRelNumMask 0x00FF          /* minor release number */
#define majorRelNumMask 0x7F00          /* major release number */
#define finalRelNumMask 0x8000          /* final release number */

/* Other Constants */
#define isFileExtended 0x8000           /* GetDirEntryGS */

/* DControl Codes */
#define resetDevice 0x0000
#define formatDevice 0x0001
#define eject 0x0002
#define setConfigParameters 0x0003
#define setWaitStatus 0x0004
#define setFormatOptions 0x0005
#define assignPartitionOwner 0x0006
#define armSignal 0x0007
#define disarmSignal 0x0008
#define setPartitionMap 0x0009
