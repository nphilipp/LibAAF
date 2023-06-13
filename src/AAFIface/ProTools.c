#include <stdio.h>

#include <libaaf/AAFIface.h>
#include <libaaf/ProTools.h>


#define PROTOOLS_CLIP_NAME_FADE_EN_LEN 		5//+1
#define PROTOOLS_CLIP_NAME_FADE_DE_LEN 		5//+1
#define PROTOOLS_CLIP_NAME_FADE_JA_LEN 		5//+1
#define PROTOOLS_CLIP_NAME_FADE_FR_LEN 		6//+1
#define PROTOOLS_CLIP_NAME_FADE_ES_LEN 		8//+1
#define PROTOOLS_CLIP_NAME_FADE_ZH_CN_LEN 3//+1
#define PROTOOLS_CLIP_NAME_FADE_ZH_TW_LEN 3//+1
#define PROTOOLS_CLIP_NAME_FADE_KO_LEN 		3//+1

#define PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_EN_LEN 		20//+1
#define PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_DE_LEN 		24//+1
#define PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ES_LEN 		32//+1
#define PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_FR_LEN 		33//+1
#define PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_JA_LEN 		 8//+1
#define PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ZH_CN_LEN  6//+1
#define PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ZH_TW_LEN  6//+1
#define PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_KO_LEN 		11//+1

/*	English : "Fade "	(Same as JA and DE)	 */
static const wchar_t PROTOOLS_CLIP_NAME_FADE_EN[] = L"\x0046\x0061\x0064\x0065\x0020\x0000";
/*	German : "Fade "	(Same as JA and EN)	 */
// static const wchar_t PROTOOLS_CLIP_NAME_FADE_DE[] = L"\x0046\x0061\x0064\x0065\x0020\x0000";
/*	Japanese : "Fade "	(Same as EN and DE)	 */
// static const wchar_t PROTOOLS_CLIP_NAME_FADE_JA[] = L"\x0046\x0061\x0064\x0065\x0020\x0000";
/*	French : "Fondu "	*/
static const wchar_t PROTOOLS_CLIP_NAME_FADE_FR[] = L"\x0046\x006f\x006e\x0064\x0075\x0020\x0000";
/*	Spanish : "Fundido"	*/
static const wchar_t PROTOOLS_CLIP_NAME_FADE_ES[] = L"\x0046\x0075\x006e\x0064\x0069\x0064\x006f\x0020\x0000";
/*	Chinese (S) : "淡变 " */
static const wchar_t PROTOOLS_CLIP_NAME_FADE_ZH_CN[] = L"\x6de1\x53d8\x0020\x0000";
/*	Chinese (T) : "淡變 " */
static const wchar_t PROTOOLS_CLIP_NAME_FADE_ZH_TW[] = L"\x6de1\x8b8a\x0020\x0000";
/*	Korean : "페이드"	*/
static const wchar_t PROTOOLS_CLIP_NAME_FADE_KO[] = L"\xd398\xc774\xb4dc\x0000";


/*	English : "Sample accurate edit"	*/
static const wchar_t PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_EN[] = L"\x0053\x0061\x006d\x0070\x006c\x0065\x0020\x0061\x0063\x0063\x0075\x0072\x0061\x0074\x0065\x0020\x0065\x0064\x0069\x0074\x0000";
/*	German : "Samplegenaue Bearbeitung"	*/
static const wchar_t PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_DE[] = L"\x0053\x0061\x006d\x0070\x006c\x0065\x0067\x0065\x006e\x0061\x0075\x0065\x0020\x0042\x0065\x0061\x0072\x0062\x0065\x0069\x0074\x0075\x006e\x0067\x0000";
/*	Spanish : "Edición con precisión de muestra"	*/
static const wchar_t PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ES[] = L"\x0045\x0064\x0069\x0063\x0069\x00f3\x006e\x0020\x0063\x006f\x006e\x0020\x0070\x0072\x0065\x0063\x0069\x0073\x0069\x00f3\x006e\x0020\x0064\x0065\x0020\x006d\x0075\x0065\x0073\x0074\x0072\x0061\x0000";
/*	French : "Modification à l'échantillon près"	*/
static const wchar_t PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_FR[] = L"\x004d\x006f\x0064\x0069\x0066\x0069\x0063\x0061\x0074\x0069\x006f\x006e\x0020\x00e0\x0020\x006c\x0027\x00e9\x0063\x0068\x0061\x006e\x0074\x0069\x006c\x006c\x006f\x006e\x0020\x0070\x0072\x00e8\x0073\x0000";
/*	Japanese : "サンプル精度編集"	*/
static const wchar_t PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_JA[] = L"\x30b5\x30f3\x30d7\x30eb\x7cbe\x5ea6\x7de8\x96c6\x0000";
/*	Chinese (S) : "精确采样编辑"	*/
static const wchar_t PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ZH_CN[] = L"\x7cbe\x786e\x91c7\x6837\x7f16\x8f91\x0000";
/*	Chinese (T) : "精確取樣編輯"	*/
static const wchar_t PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ZH_TW[] = L"\x7cbe\x78ba\x53d6\x6a23\x7de8\x8f2f\x0000";
/*	Korean : "샘플 단위 정밀 편집"	*/
static const wchar_t PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_KO[] = L"\xc0d8\xd50c\x0020\xb2e8\xc704\x0020\xc815\xbc00\x0020\xd3b8\xc9d1\x0000";




static int is_rendered_fade( const wchar_t *clipName );
static int is_sample_accurate_edit( const wchar_t *clipName );
static int replace_clipfade_with_fade( AAF_Iface *aafi, aafiTimelineItem *Item );



static int is_rendered_fade( const wchar_t *clipName ) {

	return \
		( memcmp( clipName, PROTOOLS_CLIP_NAME_FADE_EN,    PROTOOLS_CLIP_NAME_FADE_EN_LEN    ) == 0 ) ||
	  ( memcmp( clipName, PROTOOLS_CLIP_NAME_FADE_ES,    PROTOOLS_CLIP_NAME_FADE_ES_LEN    ) == 0 ) ||
	  ( memcmp( clipName, PROTOOLS_CLIP_NAME_FADE_FR,    PROTOOLS_CLIP_NAME_FADE_FR_LEN    ) == 0 ) ||
	  ( memcmp( clipName, PROTOOLS_CLIP_NAME_FADE_ZH_CN, PROTOOLS_CLIP_NAME_FADE_ZH_CN_LEN ) == 0 ) ||
	  ( memcmp( clipName, PROTOOLS_CLIP_NAME_FADE_ZH_TW, PROTOOLS_CLIP_NAME_FADE_ZH_TW_LEN ) == 0 ) ||
		( memcmp( clipName, PROTOOLS_CLIP_NAME_FADE_KO,    PROTOOLS_CLIP_NAME_FADE_KO_LEN    ) == 0 );
}



static int is_sample_accurate_edit( const wchar_t *clipName ) {

	return \
		( memcmp( clipName, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_EN,    PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_EN_LEN    ) == 0 ) ||
		( memcmp( clipName, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_DE,    PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_DE_LEN    ) == 0 ) ||
		( memcmp( clipName, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ES,    PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ES_LEN    ) == 0 ) ||
		( memcmp( clipName, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_FR,    PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_FR_LEN    ) == 0 ) ||
		( memcmp( clipName, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_JA,    PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_JA_LEN    ) == 0 ) ||
		( memcmp( clipName, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ZH_CN, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ZH_CN_LEN ) == 0 ) ||
		( memcmp( clipName, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ZH_TW, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_ZH_TW_LEN ) == 0 ) ||
		( memcmp( clipName, PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_KO,    PROTOOLS_CLIP_NAME_SAMPLE_ACCURATE_EDIT_KO_LEN    ) == 0 );
}



static int replace_clipfade_with_fade( AAF_Iface *aafi, aafiTimelineItem *Item ) {

	if ( Item->type != AAFI_AUDIO_CLIP ) {
		return -1;
	}


	aafiAudioClip *audioClip = (aafiAudioClip*)Item->data;

	aafPosition_t currentpos = audioClip->pos;
	aafPosition_t currentlen = audioClip->len;



	aafiTimelineItem *transItem = malloc( sizeof(aafiTimelineItem) + sizeof(aafiTransition) );
	aafiTransition   *trans = (aafiTransition*)transItem->data;

	memset( transItem, 0x00, sizeof(aafiTimelineItem) + sizeof(aafiTransition) );

	transItem->type = AAFI_TRANS;
	transItem->next = NULL;
	transItem->prev = NULL;

	trans->len = audioClip->len;
	trans->flags = AAFI_INTERPOL_CONSTANT;



	// printf("\n\n");
	// printf("%ls\n", audioClip->Essence->unique_file_name );

	aafiAudioClip *prevClip = NULL;
	aafiAudioClip *nextClip = NULL;

	if ( Item->prev != NULL ) {
		if ( Item->prev->type == AAFI_AUDIO_CLIP ) {
			prevClip = (aafiAudioClip*)Item->prev->data;

			// printf("PREVIOUS POS %lu\n", prevClip->pos + prevClip->len );
			// printf("CURENT   POS %lu\n\n", currentpos );

			if ( prevClip->pos + prevClip->len < currentpos - 1 ) {
				prevClip = NULL;
			}
		}
	}

	if ( Item->next != NULL ) {
		if ( Item->next->type == AAFI_AUDIO_CLIP ) {
			nextClip = (aafiAudioClip*)Item->next->data;

			if ( is_sample_accurate_edit( nextClip->Essence->file_name ) ) {
				if ( Item->next->next != NULL ) {
					nextClip = (aafiAudioClip*)Item->next->next->data;

					// printf("NEXT   POS %lu\n", nextClip->pos );
					// printf("CURENT POS %lu\n\n", currentpos + currentlen );

					if ( nextClip->pos != currentpos + currentlen + 1 ) {
						nextClip = NULL;
					}
				}
				else {
					nextClip = NULL;
				}
			}
			else {
				// nextClip = (aafiAudioClip*)Item->next->data;

				// printf("NEXT   POS %lu\n", nextClip->pos );
				// printf("CURENT POS %lu\n\n", currentpos + currentlen );

				if ( nextClip->pos != currentpos + currentlen ) {
					nextClip = NULL;
				}
			}
		}
	}



	trans->time_a  = calloc( 2, sizeof(aafRational_t) );
	trans->value_a = calloc( 2, sizeof(aafRational_t) );

	trans->time_a[0].numerator   = 0;
	trans->time_a[0].denominator = 0;
	trans->time_a[1].numerator   = 1;
	trans->time_a[1].denominator = 1;

	if ( prevClip && nextClip ) {
		// printf(":: XFADE\n");
		trans->flags |= AAFI_TRANS_XFADE;

		trans->value_a[0].numerator   = 0;
		trans->value_a[0].denominator = 0;
		trans->value_a[1].numerator   = 1;
		trans->value_a[1].denominator = 1;
	}
	else if ( prevClip ) {
		// printf(":: FADE OUT\n");
		trans->flags |= AAFI_TRANS_FADE_OUT;

		trans->value_a[0].numerator   = 1;
		trans->value_a[0].denominator = 1;
		trans->value_a[1].numerator   = 0;
		trans->value_a[1].denominator = 0;
	}
	else if ( nextClip ) {
		// printf(":: FADE IN\n");
		trans->flags |= AAFI_TRANS_FADE_IN;

		trans->value_a[0].numerator   = 0;
		trans->value_a[0].denominator = 0;
		trans->value_a[1].numerator   = 1;
		trans->value_a[1].denominator = 1;
	}



	if ( Item->prev ) {
		Item->prev->next = transItem;
		transItem->prev = Item->prev;
	}
	else {
		aafiAudioTrack *audioTrack = NULL;

		foreach_audioTrack( audioTrack, aafi ) {

			if ( audioTrack->Items == Item ) {
				audioTrack->Items = transItem;
			}
		}

		transItem->prev = NULL;
	}

	if ( Item->next ) {
		Item->next->prev = transItem;
	}


	transItem->next = Item->next;


	aafi_freeTimelineItem( &Item );


	return 0;
}



int protools_post_processing( AAF_Iface *aafi, uint32_t flags ) {

	aafiAudioTrack *audioTrack = NULL;

	foreach_audioTrack( audioTrack, aafi ) {

		aafiTimelineItem *audioItem = audioTrack->Items;

		while ( audioItem != NULL ) {

			if ( audioItem->type != AAFI_AUDIO_CLIP ) {
				audioItem = audioItem->next;
				continue;
			}

			aafiAudioClip *audioClip = (aafiAudioClip*)audioItem->data;

			wchar_t *clipName = audioClip->Essence->file_name;

			if ( (flags & PROTOOLS_PP_REPLACE_RENDERED_CLIP_FADES) && is_rendered_fade( clipName ) ) {

				replace_clipfade_with_fade( aafi, audioItem );

				audioItem = audioTrack->Items;
				continue;
			}
			else
			if ( (flags & PROTOOLS_PP_REMOVE_SAMPLE_ACCURATE_EDIT) && is_sample_accurate_edit( clipName ) ) {

				aafi_removeTimelineItem( aafi, audioItem );

				audioItem = audioTrack->Items;
				continue;
			}

			audioItem = audioItem->next;
		}
	}

	return 0;
}
