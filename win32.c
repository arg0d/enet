/** 
 @file  win32.c
 @brief ENet Win32 system specific functions
*/
#ifdef _WIN32

#define ENET_BUILDING_LIB 1
#include "enet/enet.h"
#include <windows.h>
#include <Ws2tcpip.h>
#include <mmsystem.h>

static enet_uint32 timeBase = 0;

int
enet_initialize (void)
{
    WORD versionRequested = MAKEWORD (1, 1);
    WSADATA wsaData;
   
    if (WSAStartup (versionRequested, & wsaData))
       return -1;

    if (LOBYTE (wsaData.wVersion) != 1||
        HIBYTE (wsaData.wVersion) != 1)
    {
       WSACleanup ();
       
       return -1;
    }

    timeBeginPeriod (1);

    return 0;
}

void
enet_deinitialize (void)
{
    timeEndPeriod (1);

    WSACleanup ();
}

enet_uint32
enet_host_random_seed (void)
{
    return (enet_uint32) timeGetTime ();
}

enet_uint32
enet_time_get (void)
{
    return (enet_uint32) timeGetTime () - timeBase;
}

void
enet_time_set (enet_uint32 newTimeBase)
{
    timeBase = (enet_uint32) timeGetTime () - newTimeBase;
}

int
enet_address_set_host (ENetAddress * address, const char * name)
{
    struct hostent * hostEntry;

    hostEntry = gethostbyname (name);
    if (hostEntry -> h_addrtype == AF_INET ||
        hostEntry -> h_addrtype == AF_INET6)
    {
        if (hostEntry -> h_addrtype == AF_INET) {
            // SET V4 ADDRESS
            address -> family = AF_INET;
            address -> ip.v4.host = * (enet_uint32 *) hostEntry -> h_addr_list [0];
            return 0;
        }
        else {
            // SET V6 ADDRESS
            if (hostEntry -> h_length != sizeof(enet_uint128)) {
                return -1;
            }
            address -> family = AF_INET6;
            address -> ip.v6.host = * (enet_uint128 *) hostEntry -> h_addr_list [0];
            return 0;
        }
    }
    else if (hostEntry == NULL ||
        hostEntry -> h_addrtype != AF_INET)
    {
        unsigned long host = inet_addr (name);
        if (host == INADDR_NONE)
            return -1;
        address -> family = AF_INET;
        address -> ip.v4.host = host;
        return 0;
    }

    return 0;
}

int
enet_address_get_host_ip (const ENetAddress * address, char * name, size_t nameLength)
{
    const char * addr = NULL;
    switch (address->family) {
        case AF_INET: addr = inet_ntop (AF_INET, (struct in_addr*)&address->ip.v4.host, name, nameLength); break;
        case AF_INET6: addr = inet_ntop (AF_INET6, (struct in6_addr*)& address->ip.v6.host, name, nameLength); break;
        default: return -1;
    }
    if (addr == NULL)
        return -1;
    else
    {
        size_t addrLen = strlen(addr);
        if (addrLen >= nameLength)
          return -1;
        memcpy (name, addr, addrLen + 1);
    }
    return 0;
}

int
enet_address_get_host (const ENetAddress * address, char * name, size_t nameLength)
{
    struct hostent * hostEntry = NULL;

    switch (address -> family) {
        case AF_INET:
        {
            struct in_addr in;
            in.s_addr = address -> ip.v4.host;
            hostEntry = gethostbyaddr ((char *) & in, sizeof (struct in_addr), AF_INET);
        } break;

        case AF_INET6:
        {
            struct in6_addr in;
            memcpy(in.s6_addr, address -> ip.v6.hostBytes, sizeof(enet_uint128));
            hostEntry = gethostbyaddr ((char *) & in, sizeof (struct in6_addr), AF_INET6);
        } break;

        default:
            break;
    }
    
    if (hostEntry == NULL)
      return enet_address_get_host_ip (address, name, nameLength);
    else
    {
       size_t hostLen = strlen (hostEntry -> h_name);
       if (hostLen >= nameLength)
         return -1;
       memcpy (name, hostEntry -> h_name, hostLen + 1);
    }

    return 0;
}

int
enet_socket_bind (ENetSocket socket, const ENetAddress * address)
{
	switch (address -> family) {
        case AF_INET:
        {
            struct sockaddr_in sin;
            memset (& sin, 0, sizeof (struct sockaddr_in));
            sin.sin_family = AF_INET;
            if (address != NULL)
            {
               sin.sin_port = ENET_HOST_TO_NET_16 (address -> port);
               sin.sin_addr.s_addr = address -> ip.v4.host;
            }
            else
            {
               sin.sin_port = 0;
               sin.sin_addr.s_addr = INADDR_ANY;
            }

            return bind (socket,
             (struct sockaddr *) & sin,
             sizeof (struct sockaddr_in)) == SOCKET_ERROR ? -1 : 0;
        } break;

        case AF_INET6:
        {
            struct sockaddr_in6 sin;
            memset (& sin, 0, sizeof (struct sockaddr_in6));
            sin.sin6_family = AF_INET6;
            if (address != NULL)
            {
               sin.sin6_port = ENET_HOST_TO_NET_16 (address -> port);
               memcpy(sin.sin6_addr.s6_addr, address -> ip.v6.hostBytes, sizeof(enet_uint128));
            }
            else
            {
               sin.sin6_port = 0;
               sin.sin6_addr = in6addr_any;
            }

            return bind (socket,
             (struct sockaddr6 *) & sin,
             sizeof (struct sockaddr_in6)) == SOCKET_ERROR ? -1 : 0;
        } break;

        default:
            break;
	}
}

int
enet_socket_get_address (ENetSocket socket, ENetAddress * address)
{
    struct sockaddr_storage sin;
    int sinLength = sizeof (struct sockaddr_storage);

    if (getsockname (socket, (struct sockaddr *) & sin, & sinLength) == -1)
      return -1;

	switch (sin.ss_family) {
        case AF_INET:
        {
            struct sockaddr_in * sinv4 = (struct sockaddr_in *) &sin;
            address -> ip.v4.host = (enet_uint32) sinv4->sin_addr.s_addr;
            address -> port = ENET_NET_TO_HOST_16 (sinv4->sin_port);
			address -> family = AF_INET;
        } break;

        case AF_INET6:
        {
            struct sockaddr_in6 * sinv6 = (struct sockaddr_in6 *) &sin;
            memcpy(address -> ip.v6.hostBytes, sinv6 -> sin6_addr.s6_addr, sizeof(enet_uint128));
            address -> port = ENET_NET_TO_HOST_16(sinv6 -> sin6_port);
            address -> family = AF_INET6;
        } break;
	}

    return 0;
}

int
enet_socket_listen (ENetSocket socket, int backlog)
{
    return listen (socket, backlog < 0 ? SOMAXCONN : backlog) == SOCKET_ERROR ? -1 : 0;
}

ENetSocket
enet_socket_create (ENetSocketType type)
{
    return socket (PF_INET, type == ENET_SOCKET_TYPE_DATAGRAM ? SOCK_DGRAM : SOCK_STREAM, 0);
}

int
enet_socket_set_option (ENetSocket socket, ENetSocketOption option, int value)
{
    int result = SOCKET_ERROR;
    switch (option)
    {
        case ENET_SOCKOPT_NONBLOCK:
        {
            u_long nonBlocking = (u_long) value;
            result = ioctlsocket (socket, FIONBIO, & nonBlocking);
            break;
        }

        case ENET_SOCKOPT_BROADCAST:
            result = setsockopt (socket, SOL_SOCKET, SO_BROADCAST, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_REUSEADDR:
            result = setsockopt (socket, SOL_SOCKET, SO_REUSEADDR, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_RCVBUF:
            result = setsockopt (socket, SOL_SOCKET, SO_RCVBUF, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_SNDBUF:
            result = setsockopt (socket, SOL_SOCKET, SO_SNDBUF, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_RCVTIMEO:
            result = setsockopt (socket, SOL_SOCKET, SO_RCVTIMEO, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_SNDTIMEO:
            result = setsockopt (socket, SOL_SOCKET, SO_SNDTIMEO, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_NODELAY:
            result = setsockopt (socket, IPPROTO_TCP, TCP_NODELAY, (char *) & value, sizeof (int));
            break;

        default:
            break;
    }
    return result == SOCKET_ERROR ? -1 : 0;
}

int
enet_socket_get_option (ENetSocket socket, ENetSocketOption option, int * value)
{
    int result = SOCKET_ERROR, len;
    switch (option)
    {
        case ENET_SOCKOPT_ERROR:
            len = sizeof(int);
            result = getsockopt (socket, SOL_SOCKET, SO_ERROR, (char *) value, & len);
            break;

        default:
            break;
    }
    return result == SOCKET_ERROR ? -1 : 0;
}

int
enet_socket_connect (ENetSocket socket, const ENetAddress * address)
{
    switch (address -> family) {
        case AF_INET:
        {
            struct sockaddr_in sin;
            int result;
            memset (& sin, 0, sizeof (struct sockaddr_in));

            sin.sin_family = AF_INET;
            sin.sin_port = ENET_HOST_TO_NET_16 (address -> port);
            sin.sin_addr.s_addr = address -> ip.v4.host;

            result = connect (socket, (struct sockaddr *) & sin, sizeof (struct sockaddr_in));
            if (result == SOCKET_ERROR && WSAGetLastError () != WSAEWOULDBLOCK)
              return -1;
        } break;

        case AF_INET6:
        {
            struct sockaddr_in6 sin6;
            int result;
            memset (& sin6, 0, sizeof (struct sockaddr_in6));

            sin6.sin6_family = AF_INET6;
            sin6.sin6_port = ENET_HOST_TO_NET_16 (address -> port);
            memcpy(sin6.sin6_addr.s6_addr, address->ip.v6.hostBytes, sizeof(enet_uint128));

            result = connect (socket, (struct sockaddr *) & sin6, sizeof (struct sockaddr_in6));
            if (result == SOCKET_ERROR && WSAGetLastError () != WSAEWOULDBLOCK)
              return -1;
        } break;

        default:
            break;
    }

    return 0;
}

ENetSocket
enet_socket_accept (ENetSocket socket, ENetAddress * address)
{
    SOCKET result;
    struct sockaddr_storage sin;
    int sinLength = sizeof (struct sockaddr_storage);

    result = accept (socket, 
                     address != NULL ? (struct sockaddr *) & sin : NULL, 
                     address != NULL ? & sinLength : NULL);

    if (result == INVALID_SOCKET)
      return ENET_SOCKET_NULL;

    if (address != NULL)
    {
        switch (sin.ss_family) {
            case AF_INET:
            {
                struct sockaddr_in * sinv4 = (struct sockaddr_in *) &sin;
                address -> ip.v4.host = (enet_uint32) sinv4->sin_addr.s_addr;
                address -> port = ENET_NET_TO_HOST_16 (sinv4->sin_port);
                address -> family = AF_INET;
            } break;

            case AF_INET6:
            {
                struct sockaddr_in6 * sinv6 = (struct sockaddr_in6 *) &sin;
                memcpy(address -> ip.v6.hostBytes, sinv6->sin6_addr.s6_addr, sizeof(enet_uint128));
                address -> port = ENET_NET_TO_HOST_16 (sinv6->sin6_port);
                address -> family = AF_INET6;
            } break;
        }
    }

    return result;
}

int
enet_socket_shutdown (ENetSocket socket, ENetSocketShutdown how)
{
    return shutdown (socket, (int) how) == SOCKET_ERROR ? -1 : 0;
}

void
enet_socket_destroy (ENetSocket socket)
{
    if (socket != INVALID_SOCKET)
      closesocket (socket);
}

int
enet_socket_send (ENetSocket socket,
                  const ENetAddress * address,
                  const ENetBuffer * buffers,
                  size_t bufferCount)
{
    struct sockaddr_storage sin;
    DWORD sentLength;

    if (address != NULL)
    {
        memset (& sin, 0, sizeof (struct sockaddr_storage));

        switch (address -> family) {
            case AF_INET:
            {
                struct sockaddr_in * sinv4 = (struct sockaddr_in *) &sin;
                sinv4->sin_family = AF_INET;
                sinv4->sin_port = ENET_HOST_TO_NET_16 (address -> port);
                sinv4->sin_addr.s_addr = address -> ip.v4.host;
            } break;

            case AF_INET6:
            {
                struct sockaddr_in6 * sinv6 = (struct sockaddr_in6 *) &sin;
                sinv6->sin6_family = AF_INET6;
                sinv6->sin6_port = ENET_HOST_TO_NET_16 (address -> port);
                memcpy(sinv6->sin6_addr.s6_addr, address->ip.v6.hostBytes, sizeof(enet_uint128));
            } break;
        }
    }

    if (WSASendTo (socket, 
                   (LPWSABUF) buffers,
                   (DWORD) bufferCount,
                   & sentLength,
                   0,
                   address != NULL ? (struct sockaddr *) & sin : NULL,
                   address != NULL ? sizeof (struct sockaddr_in) : 0,
                   NULL,
                   NULL) == SOCKET_ERROR)
    {
       if (WSAGetLastError () == WSAEWOULDBLOCK)
         return 0;

       return -1;
    }

    return (int) sentLength;
}

int
enet_socket_receive (ENetSocket socket,
                     ENetAddress * address,
                     ENetBuffer * buffers,
                     size_t bufferCount)
{
    INT sinLength = sizeof (struct sockaddr_storage);
    DWORD flags = 0,
          recvLength;
    struct sockaddr_storage sin;

    if (WSARecvFrom (socket,
                     (LPWSABUF) buffers,
                     (DWORD) bufferCount,
                     & recvLength,
                     & flags,
                     address != NULL ? (struct sockaddr *) & sin : NULL,
                     address != NULL ? & sinLength : NULL,
                     NULL,
                     NULL) == SOCKET_ERROR)
    {
       switch (WSAGetLastError ())
       {
       case WSAEWOULDBLOCK:
       case WSAECONNRESET:
          return 0;
       }

       return -1;
    }

    if (flags & MSG_PARTIAL)
      return -1;

    if (address != NULL)
    {
        switch (sin.ss_family) {
            case AF_INET:
            {
                struct sockaddr_in * sinv4 = (struct sockaddr_in *) &sin;
                address -> ip.v4.host = (enet_uint32) sinv4->sin_addr.s_addr;
                address -> port = ENET_NET_TO_HOST_16 (sinv4->sin_port);
                address -> family = AF_INET;
            } break;

            case AF_INET6:
            {
                struct sockaddr_in6 * sinv6 = (struct sockaddr_in6 *) &sin;
                memcpy(address->ip.v6.hostBytes, sinv6->sin6_addr.s6_addr, sizeof(enet_uint128));
                address -> port = ENET_NET_TO_HOST_16 (sinv6->sin6_port);
                address -> family = AF_INET6;
            } break;
        }
    }

    return (int) recvLength;
}

int
enet_socketset_select (ENetSocket maxSocket, ENetSocketSet * readSet, ENetSocketSet * writeSet, enet_uint32 timeout)
{
    struct timeval timeVal;

    timeVal.tv_sec = timeout / 1000;
    timeVal.tv_usec = (timeout % 1000) * 1000;

    return select (maxSocket + 1, readSet, writeSet, NULL, & timeVal);
}

int
enet_socket_wait (ENetSocket socket, enet_uint32 * condition, enet_uint32 timeout)
{
    fd_set readSet, writeSet;
    struct timeval timeVal;
    int selectCount;
    
    timeVal.tv_sec = timeout / 1000;
    timeVal.tv_usec = (timeout % 1000) * 1000;
    
    FD_ZERO (& readSet);
    FD_ZERO (& writeSet);

    if (* condition & ENET_SOCKET_WAIT_SEND)
      FD_SET (socket, & writeSet);

    if (* condition & ENET_SOCKET_WAIT_RECEIVE)
      FD_SET (socket, & readSet);

    selectCount = select (socket + 1, & readSet, & writeSet, NULL, & timeVal);

    if (selectCount < 0)
      return -1;

    * condition = ENET_SOCKET_WAIT_NONE;

    if (selectCount == 0)
      return 0;

    if (FD_ISSET (socket, & writeSet))
      * condition |= ENET_SOCKET_WAIT_SEND;
    
    if (FD_ISSET (socket, & readSet))
      * condition |= ENET_SOCKET_WAIT_RECEIVE;

    return 0;
} 

#endif

