#ifndef ESP_AWS_SHADOW_TOPIC_H_
#define ESP_AWS_SHADOW_TOPIC_H_

/**
 * @brief The maximum length of Thing Name.
 */
#define SHADOW_THINGNAME_LENGTH_MAX (128U)

/**
 * @brief The maximum length of Shadow Name.
 */
#define SHADOW_NAME_LENGTH_MAX (64U)

/**
 * @brief The maximum length of a topic name.
 */
#define SHADOW_TOPIC_MAX_LENGTH (256U)

#define SHADOW_PREFIX_CLASSIC_FORMAT "$aws/things/%s/shadow"
#define SHADOW_PREFIX_NAMED_FORMAT "$aws/things/%s/shadow/name/%s"

#define SHADOW_OP_GET "/get"
#define SHADOW_OP_GET_LENGTH (4U)

#define SHADOW_OP_UPDATE "/update"
#define SHADOW_OP_UPDATE_LENGTH (7U)

#define SHADOW_OP_DELETE "/delete"
#define SHADOW_OP_DELETE_LENGTH (7U)

#define SHADOW_SUFFIX_ACCEPTED "/accepted"
#define SHADOW_SUFFIX_ACCEPTED_LENGTH (9U)

#define SHADOW_SUFFIX_REJECTED "/rejected"
#define SHADOW_SUFFIX_REJECTED_LENGTH (9U)

#define SHADOW_SUFFIX_DOCUMENT "/documents"
#define SHADOW_SUFFIX_DOCUMENT_LENGTH (10U)

#define SHADOW_SUFFIX_DELTA "/delta"
#define SHADOW_SUFFIX_DELTA_LENGTH (6U)

#endif
