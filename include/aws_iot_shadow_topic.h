#ifndef AWS_IOT_SHADOW_TOPIC_H_
#define AWS_IOT_SHADOW_TOPIC_H_

/**
 * @brief The maximum length of Thing Name.
 */
#define AWS_IOT_SHADOW_THINGNAME_LENGTH_MAX (128U)

/**
 * @brief The maximum length of Shadow Name.
 */
#define AWS_IOT_SHADOW_NAME_LENGTH_MAX (64U)

/**
 * @brief The maximum length of a topic name.
 */
#define AWS_IOT_SHADOW_TOPIC_MAX_LENGTH (256U)

/**
 * @brief Prefix of thing name in client_id.
 */
#define AWS_IOT_SHADOW_THING_NAME_PREFIX ":thing/"

#define AWS_IOT_SHADOW_PREFIX_CLASSIC_FORMAT "$aws/things/%s/shadow"
#define AWS_IOT_SHADOW_PREFIX_NAMED_FORMAT "$aws/things/%s/shadow/name/%s"

#define AWS_IOT_SHADOW_OP_GET "/get"
#define AWS_IOT_SHADOW_OP_GET_LENGTH (4U)

#define AWS_IOT_SHADOW_OP_UPDATE "/update"
#define AWS_IOT_SHADOW_OP_UPDATE_LENGTH (7U)

#define AWS_IOT_SHADOW_OP_DELETE "/delete"
#define AWS_IOT_SHADOW_OP_DELETE_LENGTH (7U)

#define AWS_IOT_SHADOW_SUFFIX_ACCEPTED "/accepted"
#define AWS_IOT_SHADOW_SUFFIX_ACCEPTED_LENGTH (9U)

#define AWS_IOT_SHADOW_SUFFIX_REJECTED "/rejected"
#define AWS_IOT_SHADOW_SUFFIX_REJECTED_LENGTH (9U)

#define AWS_IOT_SHADOW_SUFFIX_DOCUMENT "/documents"
#define AWS_IOT_SHADOW_SUFFIX_DOCUMENT_LENGTH (10U)

#define AWS_IOT_SHADOW_SUFFIX_DELTA "/delta"
#define AWS_IOT_SHADOW_SUFFIX_DELTA_LENGTH (6U)

#endif
