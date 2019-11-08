/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Atmark Techno, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* CAUTION: This is just a temoporary! */
#define DEGU_TEST_KEY \
"-----BEGIN RSA PRIVATE KEY-----\n\
MIIEpQIBAAKCAQEAw2BJQM/ONHG0Df76vnTiGbPscEywbZnX6P4lt1gWNvP3NvvK\n\
WJz7t6miKg4VeDAgfbi77jopebmsB3qlAwWPdDV5lggGIesWCOQATZHkW5w4GkjJ\n\
0RtL07wm9Fo6Uv66djitjxo+jitl1E5Bde1EPRa+V3MUagccGZZdZtFdql96aTya\n\
/YEfqFvCSlD2zk3uIo/q+iijs0zo4lM3YtMkIyemp6h0NbsVs46WzRiMTWcwCbLb\n\
kzx+X8+2Oxb/h13yYilGLRqG4eQc1dVAPN3DRrhtWBZGTx6bVCVTPSI90/wewXWE\n\
K4hGEmMEJ0psOzB7wiWOEaST2LhtKhgRAdOtzQIDAQABAoIBAD1uAtpL/Gvk7FYS\n\
O8iye3zVY5wToM11N7vcXQN+wM1ae/pfxMmD4mlm/sP2Va3KhAzDjuNiijPpuztG\n\
xqdiki5ZufcAYt07S1xUQ/wfyQs83S72f+4thPP4Ds95pyj9SqdtPrTl7ZFJ9+R/\n\
DnoDthb3FbXtSxfjUGSDMK7pWWf9vEwslRO36MuSNwOAeDtrDkwSi0K0LADuzjm7\n\
UVIIQdH7iutdT2mDDRIugOjASrAiVmhJQ+T+GJuAiZ38ZFSbjPyudbISbHz5eTZg\n\
TpyhwaXt3jCRRE6mBC+wRcAid3zMIwbWESXQXXMIKN2/cE1wR3fTZ+aNQ51Zgf22\n\
+BETxQECgYEA70V33Y+jYB69E0v+UchMPsyI2yzB5gXtOv5CmEmHjprcvMA96zgo\n\
nzyHq1pyCgnxxmKC2I/92Vaj7NweTLfZ44lv0OvQ4kyjxoqOyjZSN3adMxiKaDbu\n\
Sgh64IeNK1n+eb84xLrA9v1SUDB4Krufs7nrf4eViB982ssuEaEf6iECgYEA0Qkr\n\
0K6UqpkP7fWoWH7lj73HXKnzlyzhEZiCrny9HMwVYbr8k7B9lXvT63thts9V00U+\n\
hS8AKLsJlUKSMzum7G4v0zhkEsM+a+m03pIU/ztT5wFC58+OelJkyKApUJp+2hHz\n\
soveM8hkUJoDc9pAF2rb6hbNpWU8RpJiqG/ixi0CgYEAnxI+GEyQPyzwYs3Y7CIP\n\
noh4S847smMqTlgPihMITiRisBHWmHSjfQnO4Hqm6kmmNU/00WkZSM6y+Jt2gWaR\n\
MxaCAhFks65kC3C+cW0fx8PRytB748DxNsLgfjlf/vw7lFEuuou2Ef5kJO+0VCSc\n\
je4CxKwtG2xjo0oE/K9z+kECgYEAonytiV5bg6zNHBzryjaBvVU45gfZqLajESlq\n\
07V0zzC4oipWcXssc4k4twBGeXL7dOmLar2ZMxAIp/SCbr68x2XzWQ2phIguOnYF\n\
OUl2dtuoIZXyav0E9IKdgvI0i0o8tshjlXNxuvDXQWwmqOSLE2jmHzWwjxN9YiIh\n\
/GpUbdUCgYEA7aUVAEqRGJvAlxMUi5oc4GryVVMed8+fgqZMZi+WzpG+rwncfGgc\n\
xmk3KeX+A7q5q3BVNa3N9qRaPekUMinspRX6OhieUGbWRanUQCzHWEdmjnKcBkRx\n\
WdA4Is7GXokHHAqdLiESHeStBDwfW2wFjNx6jLHHfQfvFROCsx7s4i0=\n\
-----END RSA PRIVATE KEY-----"

/* CAUTION: This is just a temoporary! */
#define DEGU_TEST_CERT \
"-----BEGIN CERTIFICATE-----\n\
MIIDWjCCAkKgAwIBAgIVAO8GeTlU5DEcaZT03419FSSWVG2SMA0GCSqGSIb3DQEB\n\
CwUAME0xSzBJBgNVBAsMQkFtYXpvbiBXZWIgU2VydmljZXMgTz1BbWF6b24uY29t\n\
IEluYy4gTD1TZWF0dGxlIFNUPVdhc2hpbmd0b24gQz1VUzAeFw0xOTA2MDQwNjE0\n\
NDVaFw00OTEyMzEyMzU5NTlaMB4xHDAaBgNVBAMME0FXUyBJb1QgQ2VydGlmaWNh\n\
dGUwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDDYElAz840cbQN/vq+\n\
dOIZs+xwTLBtmdfo/iW3WBY28/c2+8pYnPu3qaIqDhV4MCB9uLvuOil5uawHeqUD\n\
BY90NXmWCAYh6xYI5ABNkeRbnDgaSMnRG0vTvCb0WjpS/rp2OK2PGj6OK2XUTkF1\n\
7UQ9Fr5XcxRqBxwZll1m0V2qX3ppPJr9gR+oW8JKUPbOTe4ij+r6KKOzTOjiUzdi\n\
0yQjJ6anqHQ1uxWzjpbNGIxNZzAJstuTPH5fz7Y7Fv+HXfJiKUYtGobh5BzV1UA8\n\
3cNGuG1YFkZPHptUJVM9Ij3T/B7BdYQriEYSYwQnSmw7MHvCJY4RpJPYuG0qGBEB\n\
063NAgMBAAGjYDBeMB8GA1UdIwQYMBaAFLydd+eGrqMxsWah6QHK/kyxUNmdMB0G\n\
A1UdDgQWBBQpOcDTgJt0PPIEKkoNDx8Mdc1sTTAMBgNVHRMBAf8EAjAAMA4GA1Ud\n\
DwEB/wQEAwIHgDANBgkqhkiG9w0BAQsFAAOCAQEAIv7rV+34Y1YIJH8lmlXMTgre\n\
dNdzpS4LjagEcrCklyEJEKkyf8FGXQK9bLxpJPUzPPZWxI9bVlLkq65SOexZ7XRO\n\
7orSpGIknPMoU1FDegQoDtsvXiix64RwJ3LFi7JoaT17m5CLZP9iUH/m4tNtUArS\n\
GK8JnTj5bAaxj/J02xcTvznsEzpDjLMiTEu872fgyeHowzOaD8EvA9+HSG3ilSrA\n\
FuhwaVfVVUaqd3Kfpjmr6k7VT2HH6qXgUpfiCpYCHc6pDZV7wsjZK3Bqb01OaNaK\n\
6PS1jjAtAQmfdXHLwOStqkCEA72/FaOiUlYSCa6USEmGys4jdD64CFDr2Hpdww==\n\
-----END CERTIFICATE-----"

/* CAUTION: This is just a temoporary! */
#define DEGU_TEST_TIMEOUT_SEC "300"
