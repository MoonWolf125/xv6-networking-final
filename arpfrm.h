/**
 * Define ethernet header struct and functions to be used
 */
struct eth_head {
    uint8_t dmac[6];	// Destination MAC Address
    uint8_t smac[6];	// Sender MAC Address
    uint16_t ethtype;	// Ethernet Type
    uint16_t hwtype;	// Hardware Type
    uint16_t prottype;	// Protocol Type
    uint8_t hwsize;	// Hardware Address Length
    uint8_t protsize;	// Protocol Address Length
    uint16_t opercode;	// Operation Code
    uint8_t arpsmac[6];	// Sender MAC Address
    uint32_t sip;	// Sender IP Address
    uint8_t arpdmac[6];	// Destination MAC Address
    uint32_t dip;	// Destination IP Address
    uint16_t padd;	// Padding
};

// Initialize the ARP frame
int initframe(uint8_t * smac, char * ipadd, struct eth_head * eth);

// Convert MAC Address
void unpackmac(uint8_t * mac, char * macstr);

// Conversions for int to hex and hex to int
char inttohex(uint n);