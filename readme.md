# Instructions from GmailEncoder

## Description

GmailEncoder reads mails and attachments from your gmail account, saves them to ./content folder and encrypts all information.

### Technologies

The software uses OAuth 2.0 for authorization

https://developers.google.com/identity/protocols/OAuth2InstalledApp


### Libraries

The software requires Boost, OpenSSL, json_spirit

https://github.com/cierelabs/json_spirit

and vld

https://github.com/KindDragon/vld

### Encryption

The software encrypts a data with RSA and AES. Every file encrypts separately, file structure is not encrypted.

Encryption is done in three stages. At first stage new unique password for AES is generated. At second stage password encrypts with RSA public key. At third stage the whole encrypts with unique password. Encrypted file has the following structure

* 8 байт - length of AES password in big endian format, later N
* N байт - AES password as byte-code encrypted by RSA with OAEP padding
* 8 байт - length of encrypted file in big endian format, later N
* N байт - file encrypted with AES-256-cbc 

### Decryption

Decoder.exe utility decrypts all ./content folder.

### Platforms

The software builds for Windows 7 x64

 
## Parameters of GmailEncoder

Parameters can be passed via ini-file or command line. The software has the following parameters

Name in file | Name in command line	| Default value | Description
-------------|----------------------|---------------|------------
cfgfile | -с | gmail.cfg | Path to ini-file 
start | -s | | Staring Time in 'YYYY-mm-dd hh:mm:ss' format
end | -e | | Ending time in the same format
key | -k | | Path to RSA PEM for ssh key


## Parameters of Decoder

Параметры в программу могут предаваться через обычный инициализационный файл 
или командную строку. Используются следующие ключи

Name in file | Name in command line	| Default value | Description
-------------|----------------------|---------------|------------
cfgfile | -с | gmail.cfg | Path to ini-file 
key | -k | | Path to RSA PEM for ssh key



