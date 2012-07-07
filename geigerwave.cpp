#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define DEFAULT_THRESHOLD	(1000)
#define DEFAULT_LEAVING_TIME_THRESHOLD (0.0001)	// 100µs

/*************************************
 * Structure pour le fichier WAVE
 * Récupéré depuis http://www.high-geek.com/archives/169
 */

#if defined WIN32 && defined _MSC_VER
// définis par le compilateur Microsoft
typedef __int8 int8_t;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#else
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#endif

/*! 
* def unsigned long word 
* Variable de 4 octets 
*/  
typedef unsigned short word;  
/*! 
* def unsigned long dword 
* Variable de 4 octets 
*/  
typedef uint32_t dword;  
      
// Structure  
/** 
* struct WAVE 
* brief Structure d'un fichier WAVE standart 
* Structure codé grace aux structures RIFF, fmt et data d'un fichier audio .wav 
* 
* 
*ingroup wave 
*/  
      
typedef struct WAVE
{  
      
	/** 
	*struct RIFF 
	*brief Description principal du fichier 
	* "RIFF" (chunk descriptor) structure décrivant la nature du fichier. 
	*/  
	struct
	{  
		char ChunkID[4];    // contient les lettres "RIFF" pour indiquer que le fichier est codé selon la norme RIFF  
		word ChunkSize;        // taille du fichier entier en octets (sans compter les 8 octets de ce champ (4o) et le champ précédent CunkID (4o)  
		char Format[4];        // correspond au format du fichier donc ici, contient les lettres "WAVE" car fichier est au format wave  
	} RIFF;  
      
	/** 
	*struct fmt 
	*brief Sprécifications du format audio 
	* "fmt " (sub-chunk) structure décrivant le format des données audio 
	*/  
	struct
	{  
		char Subchunk1ID[4];    // contient les lettres "fmt " pour indiquer les données à suivre décrivent le format des données audio  
		dword Subchunk1Size;    // taille en octet des données à suivre (qui suivent cette variable) 16 Pour un fichier PCM  
		word AudioFormat;        // format de compression (une valeur autre que 1 indique une compression)  
		word NumChannels;        // nombre de canaux: Mono = 1, Stereo = 2, etc..  
		dword SampleRate;        // fréquence d'échantillonage, ex 44100, 44800 (nombre d'échantillons par secondes)  
		dword ByteRate;            // nombre d'octects par secondes  
		word Blockalign;        // nombre d'octects pour coder un échantillon  
		word BitsPerSample;    // nombre de bits pour coder un échantillon  
	} fmt;  
      
	/** 
	* 
	*struct data 
	*brief Données du fichier audio 
	* "data" (sub-chunk) contient les données audio, les échantillons et le nombre d'octets qu'ils représentent. 
	*/  
	struct
	{  
		char Subchunk2ID[4];    // contient les lettres "data" pour indiquer que les données à suivre sont les données audio (les échantillons et)  
		dword Subchunk2Size;    // taille des données audio (nombre total d'octets codant les données audio)  
	} data;  
} WAVE;  


#define INT8_MAX	(127)
#define INT8_MIN	(-128)

/* More or less an extremely crude derivative function... */
static int8_t
evolution(int32_t value1, int32_t value2, int threshold)
{
	int32_t ret = (value2 - value1) / threshold;
	if (ret > INT8_MAX) {
		ret = INT8_MAX;
	}
	if (ret < INT8_MIN) {
		ret = INT8_MIN;
	}
	return (int8_t) ret;
}


/* Naive peak detection (both positive and negative ones).
   Note that, as this function is called for every value,
   the call to evolution(a, b, th) is the same as the call
   to evolution(b, c, th) of the previous call to peakp().
   This could hence be optimized if needed.
 */
static bool
peakp(int16_t a, int16_t b, int16_t c, int th)
{
	int8_t le = evolution(a, b, th);
	int8_t ce = evolution(b, c, th);

	/* Positive peaks */
	if (le >= 0 && ce < 0 && b > th)
		return true;

#if 0
	/* Negative peaks */
	if (le <= 0 && ce > 0 && b < -th)
		return true;
#endif
	return false;
}


static bool ProcessFile(const char *zFilename, const int nThreshold);

int main(int argc, char *argv[])
{
	if (argc<2)
	{
		fprintf(stderr,"usage : %s <fichier wave>\n",argv[0]);
		return EXIT_FAILURE;
	}
	// TODO : permettre de configurer le seuil à la ligne de commande
	const int nThreshold=200;
	ProcessFile(argv[1],nThreshold);

	return EXIT_SUCCESS;
}

static bool ReadWaveFile(const char *zFilename,
						 int16_t *&pData,
						 int32_t &nSampleCount,
						 int32_t &nSampleRate)
{
	FILE *file=NULL;
	int nError=0;
	
	// dummy loop
	do
	{
		// on ouvre le fichier
		file=fopen(zFilename,"rb");
		if (!file)
		{
			nError=errno;
			fprintf(stderr,"Le fichier \"%s\" n'a pas pu etre ouvert.\n",
				zFilename);
			break;
		}

		WAVE Header;
		// on lit l'entête
		if (fread(&Header,sizeof(Header),1,file)!=1)
		{
			nError=errno;
			fprintf(stderr,"Erreur pendant la lecture de l'entete du fichier \"%s\".",
				zFilename);
			break;
		}

		nSampleCount=(int)(Header.data.Subchunk2Size + 1) / (int)Header.fmt.Blockalign;
		pData=(short *)malloc(nSampleCount*sizeof(short));
		if (!pData)
		{
			nError=errno;
			fprintf(stderr,"Erreur pendant l'allocation du buffer (%d echantillons).",
				nSampleCount);
			break;
		}
		nSampleRate=Header.fmt.SampleRate;

		//remplissage du tableau d'échantillons data[] de la structure WAVE  
		nSampleCount=fread(pData, sizeof(short), nSampleCount, file);  
		// tout est ok!
	} while (false);

	// si erreur, on laisse un petit message
	if (nError)
	{
		fprintf(stderr,"Erreur %d (%s)\n",
			nError,strerror(nError));
	}

	// on nettoie
	if (file)
	{
		// on ferme le fichier
		fclose(file);
	}

	return nError==0;
}


class IAnalyser
{
public:
	virtual ~IAnalyser() {};
	virtual bool ProcessData(const int16_t *pData,
						const int32_t nSampleCount,
						const int32_t nSampleRate) = 0;

	static IAnalyser *New();
};

#if 0
/* This structure will hold data to and from the counting algorithm.
An instance of this structure is used with the callback function
that process the audio input. */
class CCountData : public IAnalyser
{
public:
	double start_time;
	uint64_t count;
	int threshold; // detection threshold to filter noise
	int16_t last_values[2]; /* remember the last values between calls to the
					callback fct */
	double last_spl_time;   /* timestamp for the last sample processed */
	uint64_t sample_number; /* count the number of samples (= time) */

	CCountData();
	virtual ~CCountData();
	virtual bool ProcessData(const int16_t *pData,
						const int32_t nSampleCount,
						const int32_t nSampleRate);
};

CCountData::CCountData()
	: start_time(0.0)
	, count(0)
	, threshold(DEFAULT_THRESHOLD)
	, last_spl_time(0.0)
	, sample_number(0)
{
	last_values[0]=last_values[1]=0;
}

CCountData::~CCountData()
{
}

bool CCountData::ProcessData(const int16_t *pData,
						const int32_t nSampleCount,
						const int32_t nSampleRate)
{
	int16_t prev0 = last_values[0];
	int16_t prev1 = last_values[1];
	double time = start_time;
	for (int i = 0; i < nSampleCount; i++)
	{
	//	fprintf(stderr, "%d ", in[i]);
		/* The actual sample rate seems to be only an approximation of
		   SAMPLE_RATE, hence 'time + (double) i / SAMPLE_RATE' is only
		   an estimation of the sampling time. */
		if (peakp(prev0, prev1, pData[i], threshold))
		{
			count++;
			printf("%.4f\t%6d\n", time + (double) i / nSampleRate,
			       prev1);
		}
		prev0 = prev1;
		prev1 = pData[i];
		sample_number++;
	}
	last_values[0] = prev0;
	last_values[1] = prev1;
	last_spl_time = time + (nSampleCount - 1.0) / nSampleRate;

	return true;
}

IAnalyser *IAnalyser::New()
{
	return new CCountData;
}

#else
class CPeakDetector : public IAnalyser
{
public:
	double m_fStartTime;
	double m_fLastSplTime;   /* timestamp for the last sample processed */
	uint64_t m_nCount;
	uint64_t m_nSampleNumber;
	int m_nThreshold; // detection threshold to filter noise
	double m_fLeavingPeakTimeThreshold;
	CPeakDetector();
	virtual ~CPeakDetector();
	virtual bool ProcessData(const int16_t *pData,
						const int32_t nSampleCount,
						const int32_t nSampleRate);

private:
	enum EState
	{
		EState_Noise,
		EState_Peak,
		EState_Leaving,
	} m_eCurrentState;
	double m_fLeavingPeakTime;
	double m_fMaxPeakTime;
	int m_nMaxPeakAmplitude;
};

CPeakDetector::CPeakDetector()
	: m_fStartTime(0.0)
	, m_nCount(0)
	, m_nThreshold(DEFAULT_THRESHOLD)
	, m_fLastSplTime(0.0)
	, m_nSampleNumber(0)
	, m_eCurrentState(EState_Noise)
	, m_fLeavingPeakTimeThreshold(DEFAULT_LEAVING_TIME_THRESHOLD)
{
}

CPeakDetector::~CPeakDetector()
{
}

bool CPeakDetector::ProcessData(const int16_t *pData,
						const int32_t nSampleCount,
						const int32_t nSampleRate)
{
	EState eCurrentState=m_eCurrentState;
	const double fStartTime = m_fStartTime;
	double fSampleTime=fStartTime;

	for (int i = 0; i < nSampleCount; i++)
	{
		// on regarde si le sample actuel est dans la zone ou pas
		const int nAbsSample=abs(pData[i]);
		bool bAboveThreshold=nAbsSample>=m_nThreshold;
		fSampleTime=fStartTime + (double) i / nSampleRate;

	//	fprintf(stderr,"%.4lf\t%d\n", fSampleTime,pData[i]);
		switch (eCurrentState)
		{
			case EState_Noise:
				if (bAboveThreshold)
				{
					// on vient de dépasser le seuil
					// on a donc détecté un peak
					
					// on sauve les valeurs
					m_fMaxPeakTime=fSampleTime;
					m_nMaxPeakAmplitude=nAbsSample;

					// on change l'état
					eCurrentState=EState_Peak;
				}
				break;

			case EState_Leaving:
				// dans cette état, on a eu un pic et on est en dessous du seuil
				// si on reste en dessous du seuil un certain temps, on considère qu'on a quitter le pic
				// donc, on repasse en état Noise
				// si on repasse au dessus du pic, le pic n'est pas terminé
				// on revient dans l'état Peak
				if (!bAboveThreshold)
				{
					// on est en dessous du seuil
					// on regarde si ça fait longtemps
					if ( (fSampleTime-m_fLeavingPeakTime) > m_fLeavingPeakTimeThreshold)
					{
						// le pic est passé
						m_nCount++;

						printf("%.4lf\t%6d\n", m_fMaxPeakTime, m_nMaxPeakAmplitude);
						// on passe à l'état Noise
						eCurrentState=EState_Noise;
					}
					break;
				}
				// on est repassé au dessus du seuil, on n'est donc pas sorti du pic
				eCurrentState=EState_Peak;

			case EState_Peak:
				if (bAboveThreshold)
				{
					// tant qu'on est au dessus du seuil, on met seulement à jour les valeurs
					// on cherche le plus gros peak
					if (m_nMaxPeakAmplitude<nAbsSample)
					{
						m_fMaxPeakTime=fSampleTime;
						m_nMaxPeakAmplitude=nAbsSample;
					}
				}
				else
				{
					// on est redescendu en dessous du seuil
					// on passe dans l'état Leaving
					m_fLeavingPeakTime=fSampleTime;
					eCurrentState=EState_Leaving;
				}
				break;
		}
	}
	// on sauve l'état pour les échantillons suivants
	m_eCurrentState=eCurrentState;
	m_fStartTime=fSampleTime;

	return true;
}

IAnalyser *IAnalyser::New()
{
	return new CPeakDetector;
}
#endif

static bool ProcessFile(const char *zFilename, const int nThreshold)
{
	int16_t *pData;
	int32_t nSampleCount;
	int32_t nSampleRate;

	if (!ReadWaveFile(zFilename,pData,nSampleCount,nSampleRate))
	{
		return false;
	}
	
	IAnalyser *pAnalyser=IAnalyser::New();

	pAnalyser->ProcessData(pData,nSampleCount,nSampleRate);

	delete pAnalyser;

	free(pData);
	return true;
}
