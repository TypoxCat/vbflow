/**
  ******************************************************************************
  * @file    network_data_params.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-08-22T00:47:42+0700
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */

#include "network_data_params.h"


/**  Activations Section  ****************************************************/
ai_handle g_network_activations_table[1 + 2] = {
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
  AI_HANDLE_PTR(NULL),
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
};




/**  Weights Section  ********************************************************/
AI_ALIGNED(32)
const ai_u64 s_network_weights_array_u64[138] = {
  0xbecccdd1be840553U, 0xbe379b603e014e9bU, 0x3ea33f5b3d4accf6U, 0xbe893cf63eb8199cU,
  0xbe84c562bf2dad8aU, 0x3e0c9e7cbe901039U, 0xbf369bc2bdb83c62U, 0xbdbe8edcbe010349U,
  0xbea81d7ebf438ec4U, 0xbeb4d3c6be26c981U, 0x3e9d4dc4bed8549eU, 0xbf24aa29bf0f3a6fU,
  0xbe98137f3eddbcbcU, 0xbec761143e4adea3U, 0xbd353fa73f02b8c4U, 0x3ee9d872bec34ea2U,
  0x3e56429abeb3a962U, 0x3f3fc996be0fcb53U, 0xbd1d18b8be0148dcU, 0xbf1b127a3e12dff7U,
  0xbf12ea23bf71c4daU, 0xbf517a4f3e77bb4aU, 0xbf313fc1bf2c6431U, 0xbd7c629abf4abf76U,
  0xbf108e873efad74bU, 0xbecfe14ebf069aedU, 0x3d7061a7bcc312bdU, 0x3da87c973f068236U,
  0x3e8e6bff3e1ecb28U, 0x3ee9a696be1fa463U, 0xbe9f2b0a3d0a21c7U, 0x3f3a5b00beac52b7U,
  0xbf0ae0cebd37520cU, 0xbe218f36be713345U, 0xbe017bca3e99dc00U, 0xbc60f4873f1a1d33U,
  0xbf0836f9be8e6825U, 0xbbe77c70be819b04U, 0x3d8277df3ed54deaU, 0xbf3961593ec29940U,
  0xbee5920ebf7d3cc3U, 0xbe7aef43bf2ec396U, 0xbe818960be698cf6U, 0x3d6c1384bf1a38a3U,
  0xbeac81a8bf810847U, 0xbedd1563bf46ab58U, 0xbe12e1403e72cfcfU, 0xbf05ecffbeade5c8U,
  0x3ec8dc0b3e248decU, 0xbea3b0b8be9a4d42U, 0x3ea6e3373df59e94U, 0x3d2f7999be0a5902U,
  0x3ef69d88be7692e0U, 0x3f551e6bbe1af47bU, 0x3dd68d5c3f21ce24U, 0x3dae96abbecddca1U,
  0x3f01b636babc28e6U, 0xbd43fec7bf2bcb79U, 0x3ec663503e81cbacU, 0xbdb140d9bf19458aU,
  0x3eface4fbe589a57U, 0x3eedd909bf01140bU, 0xbbc5dd723ea300c0U, 0x3eed50a0bf3aba28U,
  0x3e2d99893e331b88U, 0x3e346f213ee006d4U, 0x3e7db252befa485cU, 0x3f336f353e89c345U,
  0x3d8f40963f1f8c67U, 0xbe07de4c3ee30839U, 0x3f14cdfb3d4d9d98U, 0xbe0d28a53f009b03U,
  0xbeab6b8bbeb607c8U, 0x3ef88d603f5667adU, 0x3ec2bde53e3d431cU, 0x3e2e81643ec3ee41U,
  0xbf084d3e3ea8268aU, 0xbf2a41ad3e6378f5U, 0x3e807953bf293a20U, 0x3e6241743eab74caU,
  0x3f1fdb7e3eccdb09U, 0xbd3e655c3f36743dU, 0xbede6a80beccd0f7U, 0x3f42d4993f526f24U,
  0x3cae07d9bed214b6U, 0xbea98695bef8ce0aU, 0x3edf071abc6223f8U, 0x3e43074c3ee6685eU,
  0x3a2cbc15be87b5d7U, 0x3f1ed3163f11d289U, 0x3d96e95dbd971f5dU, 0x3e3ee857bbb76498U,
  0x3e1961c83e10eb8cU, 0xbfb96e0f3d053a82U, 0xbd63131a3d04f412U, 0x3f19a3403f893fbfU,
  0xbe73119d3e955eaeU, 0xbf2416c8bf70287cU, 0x3f598877be04dde5U, 0x3eb7bc6bbeef1c0eU,
  0xbdeac2d03dc79ed0U, 0x3f29bababf210b51U, 0x3d876d083f64ec56U, 0xbe973917bda6093aU,
  0xbed0413dbe17abedU, 0xbe1bf192be5d2a70U, 0x3f3476f53d59de1dU, 0x3d846714bea0859cU,
  0x3ed192343ed587f3U, 0x3f13ed5dbf38d0e6U, 0xbeb35ac8bc6fc9ccU, 0x3da2f7553ebcd27fU,
  0x3ec2b0d1be26e001U, 0xbe220b0fbf415f95U, 0x3f5f7dc73e21154bU, 0x3dbdfff2bde1ca73U,
  0x3ec59f1cbf002f18U, 0x3f85fc8fbee7235bU, 0x3d4b28213f3f59c1U, 0xbeb7023fbf7fb78fU,
  0xbea6f8ee3f015bc9U, 0x3e7c4b2bbe8483d5U, 0x3f15f8d8be9e7f64U, 0x3f19e7413dbccf3aU,
  0x3d33b806bfcd10f2U, 0x3e650ec03f4e98e8U, 0xbfbf697e3f168681U, 0xbf3c550fbce387deU,
  0xbf27d1e03f282ce5U, 0x3ef15d89bf94fb06U, 0xbe24ed05bf896e62U, 0x3e99ce7dbf67194bU,
  0xbf6f1dce3f8a3ec3U, 0xbf9ae8433ef1c0b1U, 0x3ec1a39dbf6c4c8dU, 0x3e9662b93f17a9d0U,
  0x3e1577cdbeac19e7U, 0x3e4ed17fU,
};


ai_handle g_network_weights_table[1 + 2] = {
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
  AI_HANDLE_PTR(s_network_weights_array_u64),
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
};

