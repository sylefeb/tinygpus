
static inline int spiflash_busy()
{
  int id;
  asm volatile ("rdtime %0" : "=r"(id));
  return (id & 2);
}

static inline int spiflash_byte()
{
  int id;
  asm volatile ("rdtime %0" : "=r"(id));
  return (id >> 8)&255;
}

static inline void spiflash_init()
{
  while (spiflash_busy()) { /*nothing*/ }
}

int g_spiflash_ptr;

static inline void spiflash_read_begin(int addr)
{
  g_spiflash_ptr = addr;
}

static inline unsigned char spiflash_read_next()
{
  *SPIFLASH = g_spiflash_ptr; // read
  ++g_spiflash_ptr;
  while (spiflash_busy()) { /*nothing*/ }
  return spiflash_byte();
}

static inline void spiflash_read_end() { }

static inline unsigned char *spiflash_copy(int addr,unsigned char *dst,int len)
{
  while (len > 0) {
    *SPIFLASH = addr++;
    while (spiflash_busy()) { /*nothing*/ }
    *dst = spiflash_byte();
    ++ dst;
    -- len;
  }
}
