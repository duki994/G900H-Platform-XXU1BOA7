#ifndef WBS_H_
#define WBS_H_
#ifdef __cplusplus
extern "C" {
#endif
/*
 * This method encrypts a plaintext (pt) of any size in CBC mode, outputs a ciphertext (ct), and returns the size of the ciphertext (ct).
 *
 * @param ct an output ciphertext
 * @param pt an input plaintext to be encrypted
 * @param size the size of a plaintext (pt) in bytes
 * @param iv initialization vector
 * @return the size of a ciphertext (ct)
 */
long WBS_Enc (unsigned char *ct, unsigned char *pt, long size, unsigned char *iv);

/*
 * This method decrypts a ciphertext of any size in CBC mode, outputs a plaintext (pt), and returns the size of the plaintext.
 *
 * @param pt an output plaintext to be decrypted
 * @param ct an input ciphertext
 * @param size the size of a ciphertext (ct) in bytes
 * @param iv initialization vector
 * @return the size of a plaintext (pt)
 */
long WBS_Dec (unsigned char *pt, unsigned char *ct, long size, unsigned char *iv);

#ifdef __cplusplus
}
#endif

#endif /* WBS_H_ */
