/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_ifinfo.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retro_miscellaneous.h>

#if defined(_WIN32) && !defined(_XBOX)
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#elif !defined(VITA)  // QuyenNC mod
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#ifdef WANT_IFADDRS
#include <compat/ifaddrs.h>
#elif !defined(VITA)  // QuyenNC mod
#include <ifaddrs.h>
#endif
#endif

#include <net/net_ifinfo.h>

#if defined(BSD)
#include <netinet/in.h>
#endif

// QuyenNC add start
#if defined(VITA)
#include <psp2/net/netctl.h>
#endif
// QuyenNC add end

void net_ifinfo_free(net_ifinfo_t *list)
{
   unsigned k;

   if (!list)
      return;

   for (k = 0; k < list->size; k++)
   {
      struct net_ifinfo_entry *ptr =
         (struct net_ifinfo_entry*)&list->entries[k];

      if (*ptr->name)
         free(ptr->name);
      if (*ptr->host)
         free(ptr->host);

      ptr->name = NULL;
      ptr->host = NULL;
   }
   free(list->entries);
}

bool net_ifinfo_new(net_ifinfo_t *list)
{
   unsigned k              = 0;
#if defined(_WIN32) && !defined(_XBOX)
   PIP_ADAPTER_ADDRESSES adapter_addresses = NULL, aa = NULL;
   PIP_ADAPTER_UNICAST_ADDRESS ua = NULL;
#ifdef _WIN32_WINNT_WINXP
   DWORD size;
   DWORD rv = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &size);

   adapter_addresses = (PIP_ADAPTER_ADDRESSES)malloc(size);

   rv = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, adapter_addresses, &size);

   memset(list, 0, sizeof(net_ifinfo_t));

   if (rv != ERROR_SUCCESS)
      goto error;
#endif
   for (aa = adapter_addresses; aa != NULL; aa = aa->Next)
   {
      char name[PATH_MAX_LENGTH];
      memset(name, 0, sizeof(name));

      WideCharToMultiByte(CP_ACP, 0, aa->FriendlyName, wcslen(aa->FriendlyName),
            name, PATH_MAX_LENGTH, NULL, NULL);

      for (ua = aa->FirstUnicastAddress; ua != NULL; ua = ua->Next)
      {
         char host[PATH_MAX_LENGTH];
         struct net_ifinfo_entry *ptr = (struct net_ifinfo_entry*)
            realloc(list->entries, (k+1) * sizeof(struct net_ifinfo_entry));

         if (!ptr)
            goto error;

         list->entries          = ptr;

         memset(host, 0, sizeof(host));

         getnameinfo(ua->Address.lpSockaddr, ua->Address.iSockaddrLength,
               host, sizeof(host), NULL, NI_MAXSERV, NI_NUMERICHOST);

         list->entries[k].name  = strdup(name);
         list->entries[k].host  = strdup(host);
         list->size             = k + 1;

         k++;
      }
   }

   free(adapter_addresses);
// QuyenNC add start
#elif defined(VITA)
   memset(list, 0, sizeof(net_ifinfo_t));
   list->entries = (struct net_ifinfo_entry*) malloc(sizeof(struct net_ifinfo_entry));
   list->size = 1;

   SceNetCtlInfo info;
   if (sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info) >= 0) {
      list->entries[0].name = (char*) malloc(32 + 1);
      sprintf(list->entries[0].name, "WIFI_LAN");
      list->entries[0].host = (char*) malloc(32 + 1);
      strlcpy(list->entries[0].host, info.ip_address, 32);
   } else {
      goto error;
   }
// QuyenNC add end
#else
   struct ifaddrs *ifa     = NULL;
   struct ifaddrs *ifaddr  = NULL;

   memset(list, 0, sizeof(net_ifinfo_t));

   if (getifaddrs(&ifaddr) == -1)
      goto error;

   if (!list)
      goto error;

   for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
   {
      char host[NI_MAXHOST];
      struct net_ifinfo_entry *ptr = NULL;

      if (!ifa->ifa_addr)
         continue;

      if (ifa->ifa_addr->sa_family != AF_INET)
         continue;

      if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
               host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) != 0)
         goto error;

      ptr = (struct net_ifinfo_entry*)
         realloc(list->entries, (k+1) * sizeof(struct net_ifinfo_entry));

      if (!ptr)
         goto error;

      list->entries          = ptr;

      list->entries[k].name  = strdup(ifa->ifa_name);
      list->entries[k].host  = strdup(host);
      list->size             = k + 1;

      k++;
   }

   freeifaddrs(ifaddr);
#endif
   return true;

error:
#ifdef _WIN32
   if (adapter_addresses)
      free(adapter_addresses);
#elif !defined(VITA)  // QuyenNC mod
   freeifaddrs(ifaddr);
#endif
   net_ifinfo_free(list);

   return false;
}
