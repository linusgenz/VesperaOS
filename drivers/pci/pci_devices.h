//
// Created by linus on 01.07.25.
//

#ifndef PCI_DEVICES_H
#define PCI_DEVICES_H
#include <vespera/types.h>

struct PciDevice {
    u16 vendor_id;
    u16 device_id;
    const char* name;
};

static constexpr PciDevice PCI_DEVICES[] = {
    {0x8086, 0x29C0, "Express DRAM Controller"                                                            },
    {0x8086, 0x2918, "LPC Interface Controller"                                                           },
    {0x8086, 0x2922, "6 port SATA Controller [AHCI mode]"                                                 },
    {0x8086, 0x2930, "SMBus Controller"                                                                   },
    {0x8086, 0x5917, "UHD Graphics 620"                                                                   },
    {0x8086, 0x9D2F, "Sunrise Point-LP USB 3.0 xHCI Controller"                                           },
    {0x8086, 0x9D23, "Sunrise Point-LP SMBus"                                                             },
    {0x8086, 0x15C1, "JHL6240 Thunderbolt 3 USB 3.1 Controller (Low Power) [Alpine Ridge LP 2016]"        },
    {0x10dE, 0x2705, "AD103 [GeForce RTX 4070 Ti SUPER]"                                                  },
    {0x8086, 0x5914, "Xeon E3-1200 v6/7th Gen Core Processor Host Bridge/DRAM Registers"                  },
    {0x8086, 0x1903, "Xeon E3-1200 v5/E3-1500 v5/6th Gen Core Processor Thermal Subsystem"                },
    {0x8086, 0x1911, "Xeon E3-1200 v5/v6 / E3-1500 v5 / 6th/7th Gen Core Processor Gaussian Mixture Model"},
    {0x8086, 0x9D31, "Sunrise Point-LP Thermal subsystem"                                                 },
    {0x8086, 0x9D3A, "Sunrise Point-LP CSME HECI #1"                                                      },
    {0x8086, 0x9D3D, "Sunrise Point-LP Active Management Technology - SOL"                                },
    {0x8086, 0x9D10, "Sunrise Point-LP PCI Express Root Port #1"                                          },
    {0x8086, 0x9D16, "Sunrise Point-LP PCI Express Root Port #7"                                          },
    {0x8086, 0x9D18, "Sunrise Point-LP PCI Express Root Port #9"                                          },
    {0x8086, 0x9D1A, "Sunrise Point-LP PCI Express Root Port #11"                                         },
    {0x8086, 0x9D4E, "Intel(R) 100 Series Chipset Family LPC Controller/eSPI Controller - 9D4E"           },
    {0x8086, 0x9D21, "Sunrise Point-LP PMC"                                                               },
    {0x8086, 0x15D7, "Ethernet Connection (4) I219-LM"                                                    },
    {0x8086, 0x24FD, "Wireless 8265 / 8275"                                                               },
    {0x8086, 0x15C0, "JHL6240 Thunderbolt 3 Bridge (Low Power) [Alpine Ridge LP 2016]"                    },
    {0x8086, 0x15bf, "JHL6240 Thunderbolt 3 NHI (Low Power) [Alpine Ridge LP 2016]"                       },
    {0x8086, 0x9D23, "Ethernet Connection (4) I219-LM"                                                    },
    {0x8086, 0x9D71, "Sunrise Point-LP HD Audio"                                                          },
    {0x1022, 0x43F5, "600 Series Chipset PCIe Switch Downstream Port"                                     },
    {0x1022, 0x43F4, "600 Series Chipset PCIe Switch Upstream Port"                                       },
    {0x1022, 0x43F7, "600 Series Chipset USB 3.2 Controller"                                              },
    {0x1022, 0x43F6, "600 Series Chipset SATA Controller"                                                 },
    {0x1022, 0x790B, "FCH SMBus Controller"                                                               },
    {0x1022, 0x790E, " FCH LPC Bridge"                                                                    },
    {0x1B21, 0x0612, "ASM1062 Serial ATA Controller"                                                      },
    {0x10EC, 0x8125, "RTL8125 2.5GbE Controller"                                                          },
    {0x144D, 0xA80A, "NVMe SSD Controller PM9A1/PM9A3/980PRO"                                             },
    {0x144D, 0xA808, "NVMe SSD Controller SM981/PM981"                                                    },
    {0x1002, 0x164E, "Raphael"                                                                            },
    {0x1002, 0x1640, "Rembrandt Radeon High Definition Audio Controller"                                  },
    {0x1002, 0x1649, "VanGogh PSP/CCP"                                                                    },
    {0x1B36, 0x000D, "QEMU XHCI Host Controller"                                                          },
    {0x8086, 0x15c1, "JHL6240 Thunderbolt 3 USB 3.1 Controller (Low Power) [Alpine Ridge LP 2016]"        },
    {0x8086, 0x9d2f, "Sunrise Point-LP USB 3.0 xHCI Controller"                                           },
    {0x046d, 0xc31c, "Keyboard K120"                                                                      }
};

#endif  // PCI_DEVICES_H