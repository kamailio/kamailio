#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <wolfssl/wolfcrypt/cryptocb.h>
#include <wolfssl/wolfcrypt/logging.h>
#include <wolfssl/wolfcrypt/wc_pkcs11.h>

static void usage(const char *prog)
{
	fprintf(stderr,
			"Usage: %s <module> <token> <object> <pin> <certificate>\n",
			prog);
}

int main(int argc, char **argv)
{
	WOLFSSL_CTX *ctx = NULL;
	WOLFSSL *ssl = NULL;
	Pkcs11Dev dev;
	Pkcs11Token tok;
	int dev_id = 1000;
	int ret;
	int registered = 0;
	int tok_inited = 0;
	int dev_inited = 0;

	const char *module;
	const char *token;
	const char *object;
	const char *pin;
	const char *cert_file;

	if(argc != 6) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	module = argv[1];
	token = argv[2];
	object = argv[3];
	pin = argv[4];
	cert_file = argv[5];

	if(wolfSSL_Init() != WOLFSSL_SUCCESS) {
		fprintf(stderr, "wolfSSL_Init failed\n");
		return EXIT_FAILURE;
	}

	memset(&dev, 0, sizeof(dev));
	memset(&tok, 0, sizeof(tok));

	ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
	if(ctx == NULL) {
		fprintf(stderr, "wolfSSL_CTX_new failed\n");
		goto error;
	}

	ret = wc_Pkcs11_Initialize(&dev, module, NULL);
	if(ret != 0) {
		fprintf(stderr, "wc_Pkcs11_Initialize failed: %d\n", ret);
		goto error;
	}
	dev_inited = 1;

	ret = wc_Pkcs11Token_InitName(&tok, &dev, token, (int)strlen(token),
			(const unsigned char *)pin, (int)strlen(pin));
	if(ret != 0) {
		fprintf(stderr, "wc_Pkcs11Token_InitName failed: %d\n", ret);
		goto error;
	}
	tok_inited = 1;

	ret = wc_CryptoCb_RegisterDevice(dev_id, wc_Pkcs11_CryptoDevCb, &tok);
	if(ret != 0) {
		fprintf(stderr, "wc_CryptoCb_RegisterDevice failed: %d\n", ret);
		goto error;
	}
	registered = 1;

	if(wolfSSL_CTX_SetDevId(ctx, dev_id) != WOLFSSL_SUCCESS) {
		fprintf(stderr, "wolfSSL_CTX_SetDevId failed\n");
		goto error;
	}

	if(wolfSSL_CTX_use_PrivateKey_Label(ctx, object, dev_id)
			!= WOLFSSL_SUCCESS) {
		fprintf(stderr, "wolfSSL_CTX_use_PrivateKey_Label failed\n");
		wc_ERR_print_errors_fp(stderr);
		goto error;
	}

	if(wolfSSL_CTX_use_certificate_chain_file(ctx, cert_file)
			!= WOLFSSL_SUCCESS) {
		fprintf(stderr, "wolfSSL_CTX_use_certificate_file failed: %s\n",
				cert_file);
		wc_ERR_print_errors_fp(stderr);
		goto error;
	}

	ssl = wolfSSL_new(ctx);
	if(ssl == NULL) {
		fprintf(stderr, "wolfSSL_new failed\n");
		goto error;
	}

	if(wolfSSL_CTX_check_private_key(ctx) != WOLFSSL_SUCCESS) {
		fprintf(stderr, "wolfSSL_CTX_check_private_key failed\n");
		wc_ERR_print_errors_fp(stderr);
		goto error;
	}

	printf("SUCCESS: PKCS#11 key + certificate verified\n");

	wolfSSL_free(ssl);
	wolfSSL_CTX_free(ctx);
	if(registered)
		wc_CryptoCb_UnRegisterDevice(dev_id);
	if(tok_inited)
		wc_Pkcs11Token_Final(&tok);
	if(dev_inited)
		wc_Pkcs11_Finalize(&dev);
	wolfSSL_Cleanup();
	return EXIT_SUCCESS;

error:
	if(ssl != NULL)
		wolfSSL_free(ssl);
	if(ctx != NULL)
		wolfSSL_CTX_free(ctx);
	if(registered)
		wc_CryptoCb_UnRegisterDevice(dev_id);
	if(tok_inited)
		wc_Pkcs11Token_Final(&tok);
	if(dev_inited)
		wc_Pkcs11_Finalize(&dev);
	wolfSSL_Cleanup();
	return EXIT_FAILURE;
}
