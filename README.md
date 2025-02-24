# SimpleRemoteDesktop

## A simple Remote Desktop for support

This project was developped to provide support to distant users.

### Current main characteristics and features

- Point-to-point connection.

- Point-to-point encrypted link.

- Works in reverse mode. (No need to open network port at the support requester side.)

- Control/viewer software : Linux/Windows, based on SDL2.

- Target/Support requester side software : Linux. (Windows support planned).

- Implemented in C.

- I will try to keep it aligned with the 'KISS' principle. :)

## How to build it ?

To build it for Linux :

```c
make
```

or


```c
make TARGET=Linux
```

To build it for Windows :

```c
make TARGET=mingw64
```

## How to launch it ?

Support provider side (to be launched first):

```c
./SRDctrl -password:"please_use_a_strong_password" -port:port_number
```

Support requester side:

```c
./SRDtgt -password:"please_use_a_strong_password" -port:port_number -address:"support_provider_address"
```

