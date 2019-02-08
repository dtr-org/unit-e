## PGP keys of Gitian builders and Developers

The file keys.txt contains the public keys of unit-e Gitian builders and active developers.

The keys are used to sign the results of unit-e Gitian builds.

The most recent version of each pgp key can be found on pgp servers in the sks key server pool.

Fetch the latest version from the key server to see if any key was revoked in
the meantime.
To fetch the latest version of all pgp keys in your gpg homedir:

```sh
gpg --refresh-keys
```

To fetch keys of unit-e builders and active developers, feed the list of
fingerprints of the primary keys into gpg:

```sh
while read fingerprint keyholder_name; do gpg --keyserver hkp://subset.pool.sks-keyservers.net --recv-keys ${fingerprint}; done < ./keys.txt
```

Add your key to the list if you provided unit-e Gitian signatures for two consecutive releases.
