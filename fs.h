// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO 1  // root i-number
#define BSIZE 512  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

/*
Además tendremos que reducir el número de bloques directos,
para implementar el doblemente indirecto, 
ya que se nos dice que no podemos cambiar el tamaño del nodo-i en disco,
por lo que prescindiremos de uno directo para implementar el indirecto
*/
#define NDIRECT 11
#define NINDIRECT (BSIZE / sizeof(uint))
//#define MAXFILE (NDIRECT + NINDIRECT) ANTES DEL EJERCICIO 1
//Ahora tenemos : Directo +   BSI      +  BDI    
#define MAXFILE (NDIRECT + NINDIRECT + NINDIRECT*NINDIRECT)


/*
   En la estructura del nodo-i, 
   ahora en vez de 12 bloques directos y un BSI , 
   sacrificamos un bloque directo para tener el BDI,
   por lo que el número de “entradas en el bloque i”
   es N Bloques directos + 1 BSI + 1 BDI

*/


// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  //uint addrs[NDIRECT+1];   // Data block addresses
  uint addrs[NDIRECT+1+1];   // Data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) (b/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

