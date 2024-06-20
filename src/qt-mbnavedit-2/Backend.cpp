#include <iostream>
#include <QFileDialog>
#include <QQuickItem>
#include <QDebug>
#include <QDir>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <QTextStream>
#include <QMetaObject>
#include <QQmlProperty>
#include <QFont>
#include "PixmapDrawer.h"
#include "Backend.h"
#include "GuiNames.h"

extern "C" {
#include "mb_status.h"

#include "mb_define.h"
#include "mb_aux.h"
#include "mb_format.h"
#include "mb_io.h"
#include "mb_process.h"
#include "mb_status.h"
}

Backend::Ping Backend::ping[MBNAVEDIT_BUFFER_SIZE] = {};

/* Mode value defines */
#define EDIT_MODE_PICK 0
#define EDIT_MODE_SELECT 1
#define EDIT_MODE_DESELECT 2
#define EDIT_MODE_SELECTALL 3
#define EDIT_MODE_DESELECTALL 4
#define OUTPUT_MODE_OUTPUT 0
#define OUTPUT_MODE_BROWSE 1
#define PLOT_TINT 0
#define PLOT_LONGITUDE 1
#define PLOT_LATITUDE 2
#define PLOT_SPEED 3
#define PLOT_HEADING 4
#define PLOT_DRAFT 5
#define PLOT_ROLL 6
#define PLOT_PITCH 7
#define PLOT_HEAVE 8
#define MODEL_MODE_OFF 0
#define MODEL_MODE_MEAN 1
#define MODEL_MODE_DR 2
#define MODEL_MODE_INVERT 3
#define NUM_FILES_MAX 1000

using namespace mb_system;

Backend::Backend(int argc, char **argv) {

  // Should give unused variable warning/error_
  programName_ = "MBNAVEDIT";
  helpMessage_ = "MBNAVEDIT is an interactive navigation editor for swath sonar data.\n\tIt can work with any data "
    "format supported by the MBIO library.\n";

  usageMessage_ = "mbnavedit [-Byr/mo/da/hr/mn/sc -D  -Eyr/mo/da/hr/mn/sc \n\t-Fformat -Ifile -Ooutfile -X -V -H]";

  error_ = MB_ERROR_NO_ERROR;
  message_ = nullptr;
  imbioPtr_ = nullptr;
  useLockFiles_ = true;
  storePtr_ = nullptr;
  beamFlag_ = nullptr;
  bath_ = nullptr;
  bathAcrossTrack_ = nullptr;
  bathAlongTrack_ = nullptr;
  amp_ = nullptr;
  ss_ = nullptr;
  ssAcrossTrack_ = nullptr;
  ssAlongTrack_ = nullptr;
  fileOpen_ = false;
  nfileOpen_ = false;
  holdSize_ = 100;
  nLoad_ = 0;
  nDump_ = 0;
  nBuff_ = 0;
  currentId_ = 0;
  nLoadTotal_ = 0;
  nDumpTotal_ = 0;
  firstRead_ = true;

  dataPlotted_ = false;

  const int width = DEFAULT_PLOT_WIDTH;
  const int height = NUMBER_PLOTS_MAX * DEFAULT_PLOT_HEIGHT;
  canvasPixmap_ = new QPixmap(width, height);

  painter_ = new QPainter(canvasPixmap_);
  QFont myFont("Helvetica [Cronyx]", 9);
  painter_->setFont(myFont);

  bool inputSpecd = false;

  init_globals();

  init(argc, argv, &inputSpecd);
}


Backend::~Backend()
{
  // Free unneeded memory
}


bool Backend::initialize(QObject *loadedRoot, int argc, char **argv) {
  ui_ = loadedRoot;

  // Find PixmapImage in QML object tree
  qDebug() << "Find PixmapImage " << SWATH_PIXMAP_NAME;

  swathPixmapImage_ =
    ui_->findChild<mb_system::PixmapImage*>(SWATH_PIXMAP_NAME);

  if (!swathPixmapImage_) {
    qCritical() << "Couldn't find " << SWATH_PIXMAP_NAME << " in QML";
    return false;
  }

  // Set the pixmap of QML-declared PixmapImage
  swathPixmapImage_->setImage(canvasPixmap_);

  swathPixmapImage_->setWidth(canvasPixmap_->width());
  swathPixmapImage_->setHeight(canvasPixmap_->height());
  
  char *swathFile = nullptr;
  // Parse command line options
  for (int i = 1; i < argc; i++) {
    // Input swath file is last argument
    if (i == argc-1) {
      swathFile = argv[i];
    }
  }

  if (swathFile) {
    char *fullPath = realpath(swathFile, nullptr);
    if (!fullPath) {
      qWarning() << "swath file \"%s\" not found: " <<  swathFile;
      return false;
    }

    QString urlString("file://" + QString(fullPath));
    if (!processSwathFile(urlString)) {
      qWarning() << "Couldn't process_ " << swathFile;
    }
  }
  else {
    plotTest();
  }

  return true;
}



void Backend::onMainWindowDestroyed() {
  qDebug() << "*** onMainWindowDestroyed() *****";
}


bool Backend::plotSwath(void) {
  if (!dataPlotted_) {
    std::cerr << "No data plotted yet\n";
    return false;
  }

  int status = MB_SUCCESS;

  if (status != MB_SUCCESS) {
    std::cerr << "mbedit_action_plot() failed\n";
    return false;
  }

  // Update GUI
  swathPixmapImage_->update();

  return true;
}


bool Backend::processSwathFile(QUrl fileUrl) {

  qDebug() << "processSwathFile() " <<  fileUrl;
  char *swathFile = strdup(fileUrl.toLocalFile().toLatin1().data());
  int format_;
  int format_Err;
  if (!mb_get_format(0, swathFile, NULL, &format_, &format_Err)) {
    std::cerr << "Couldn't determine sonar format_ of " << swathFile
	      << "\n";

    return false;
  }

  qDebug() << "Invoke mbnavedit_prog functions to open and plot data";

  // Open swath file and plot data
  strcpy(ifile_, swathFile);
  int status = action_open(false);

  if (status != MB_SUCCESS) {
    std::cerr << "mbedit_action_open() failed\n";
    return false;
  }

  // Update GUI
  swathPixmapImage_->update();

  dataPlotted_ = true;
  return true;
}


bool Backend::plotTest() {
  qDebug() << "plotTest(): canvas width: " << canvasPixmap_->width() <<
    ", canvas height: " << canvasPixmap_->height();

  painter_->eraseRect(0, 0, canvasPixmap_->width(), canvasPixmap_->height());

  //// TEST TEST TEST
  PixmapDrawer::fillRectangle(painter_, 0, 0, canvasPixmap_->width(),
			      canvasPixmap_->height(),
			      WHITE, SOLID_LINE);

  PixmapDrawer::fillRectangle(painter_, 100, 100,
			      canvasPixmap_->width()-200,
			      canvasPixmap_->height()-200,
			      RED, SOLID_LINE);

  PixmapDrawer::drawLine(painter_, 0, 0, canvasPixmap_->width(),
			 canvasPixmap_->height(),
			 BLACK, SOLID_LINE);

  PixmapDrawer::drawLine(painter_, canvasPixmap_->width(), 0, 0,
			 canvasPixmap_->height(),
			 GREEN, DASH_LINE);

  PixmapDrawer::drawString(painter_, 100, 100, (char *)"this is coral",
			   CORAL, SOLID_LINE);

  PixmapDrawer::drawString(painter_, 300, 100, (char *)"BLUE!",
			   BLUE, SOLID_LINE);


  PixmapDrawer::drawString(painter_, 400, 100, (char *)"PURPLE",
			   PURPLE, SOLID_LINE);

  // Update GUI
  swathPixmapImage_->update();


  return true;
}


void Backend::setPlot(QString plotName, bool set) {
  qDebug() << "setPlot(): " << plotName << " " << set;
  if (plotName == TIMEINT_PLOTNAME) {
    plotTint_ = set;
  }
  else if (plotName == LAT_PLOTNAME) {
    plotLat_ = set;
  }
  else if (plotName == LON_PLOTNAME) {
    plotLon_ = set;
  }
  else if (plotName == SPEED_PLOTNAME) {
    plotSpeed_ = set;
  }
  else if (plotName == HEADING_PLOTNAME) {
    plotHeading_ = set;
  }
  else if (plotName == SENSORDEPTH_PLOTNAME) {
    plotDraft_ = set;
  }
  else if (plotName == ATTITUDE_PLOTNAME) {
    plotRoll_ = set;
    plotPitch_ = set;
    plotHeave_ = set;
  }
  else {
    qWarning() << "setPlot(): Unhandled plot " << plotName;
  }

  qDebug() << "\ntimeInt_: " << plotTint_ <<
    ", plotLat_: " << plotLat_ << ", plotLon_: " << plotLon_ <<
    ", plotSpeed_: " << plotSpeed_ << ", plotHeading_: " << plotHeading_ <<
    ", plotDraft_: " << plotDraft_;

  qDebug() << "plotRoll_: " << plotRoll_ << ", plotPitch_: " << plotPitch_ <<
    ", plotHeave: " << plotHeave_;
  
  plot_all();
  
  swathPixmapImage_->update();
}



/*--------------------------------------------------------------------*/
int Backend::init_globals(void) {
  /* set default global control parameters */
  outputMode_ = OUTPUT_MODE_OUTPUT;
  runMBProcess_ = false;
  guiMode_ = false;
  dataShowMax_ = 2000;
  dataShowSize_ = 1000;
  dataStepMax_ = 2000;
  dataStepSize_ = 750;
  modePick_ = EDIT_MODE_PICK;
  modeSetInterval_ = false;
  plotTint_ = true;
  plotTintOrg_ = true;
  plotLon_ = true;
  plotLonOrg_ = true;
  plotLonDr_ = false;
  plotLat_ = true;
  plotLatOrg_ = true;
  plotLatDr_ = false;
  plotSpeed_ = true;
  plotSpeedOrg_ = true;
  plotSmg_ = true;
  plotHeading_ = true;
  plotHeadingOrg_ = true;
  plotCmg_ = true;
  plotDraft_ = true;
  plotDraftOrg_ = true;
  plotDraftDr_ = false;
  plotRoll_ = false;
  plotPitch_ = false;
  plotHeave_ = false;
  meanTimeWindow_ = 100;
  driftLon_ = 0;
  driftLat_ = 0;
  strcpy(ifile_, "");
  plotWidth_ = DEFAULT_PLOT_WIDTH;
  plotHeight_ = DEFAULT_PLOT_HEIGHT;
  nPlots_ = 0;
  if (plotTint_)
    nPlots_++;
  if (plotLon_)
    nPlots_++;
  if (plotLat_)
    nPlots_++;
  if (plotSpeed_)
    nPlots_++;
  if (plotHeading_)
    nPlots_++;
  if (plotDraft_)
    nPlots_++;
  timestampProblem_ = false;
  usePingData_ = false;
  stripComments_ = false;
  modelMode_ = MODEL_MODE_OFF;
  weightSpeed_ = 100.0;
  weightAccel_ = 100.0;
  scrollCount_ = 0;
  offsetLon_ = 0.0;
  offsetLat_ = 0.0;
  offsetLonApplied_ = 0.0;
  offsetLatApplied_ = 0.0;

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}

/*--------------------------------------------------------------------*/
int Backend::init(int argc, char **argv, bool *startup_file) {

  int status =
    mb_defaults(verbose_, &format_, &nPings_, &lonFlip_, bounds_,
		btime_i_, etime_i_, &speedMin_, &timeGap_);

  status = mb_uselockfiles(verbose_, &useLockFiles_);
  nPings_ = 1;
  lonFlip_ = 0;
  bounds_[0] = -360.;
  bounds_[1] = 360.;
  bounds_[2] = -90.;
  bounds_[3] = 90.;
  btime_i_[0] = 1962;
  btime_i_[1] = 2;
  btime_i_[2] = 21;
  btime_i_[3] = 10;
  btime_i_[4] = 30;
  btime_i_[5] = 0;
  btime_i_[6] = 0;
  etime_i_[0] = 2062;
  etime_i_[1] = 2;
  etime_i_[2] = 21;
  etime_i_[3] = 10;
  etime_i_[4] = 30;
  etime_i_[5] = 0;
  etime_i_[6] = 0;
  speedMin_ = 0.0;
  timeGap_ = 1000000000.0;
  strcpy(ifile_, "");

  int fileflag = 0;

  /* parsing variables */
  extern char *optarg;
  int errflg = 0;
  int c;
  int help = 0;

  /* process argument list */
  while ((c = getopt(argc, argv, "VvHhB:b:DdE:e:F:f:GgI:i:NnPpXx")) != -1)
    switch (c) {
    case 'H':
    case 'h':
      help++;
      break;
    case 'V':
    case 'v':
      verbose_++;
      break;
    case 'B':
    case 'b':
      sscanf(optarg, "%d/%d/%d/%d/%d/%d", &btime_i_[0], &btime_i_[1], &btime_i_[2], &btime_i_[3], &btime_i_[4], &btime_i_[5]);
      btime_i_[6] = 0;
      break;
    case 'D':
    case 'd':
      outputMode_ = OUTPUT_MODE_BROWSE;
      break;
    case 'E':
    case 'e':
      sscanf(optarg, "%d/%d/%d/%d/%d/%d", &etime_i_[0], &etime_i_[1], &etime_i_[2], &etime_i_[3], &etime_i_[4], &etime_i_[5]);
      etime_i_[6] = 0;
      break;
    case 'F':
    case 'f':
      sscanf(optarg, "%d", &format_);
      break;
    case 'G':
    case 'g':
      guiMode_ = true;
      break;
    case 'I':
    case 'i':
      sscanf(optarg, "%s", ifile_);
      parseInputDataList(ifile_, format_);
      fileflag++;
      break;
    case 'N':
    case 'n':
      stripComments_ = true;
      break;
    case 'P':
    case 'p':
      usePingData_ = true;
      break;
    case 'X':
    case 'x':
      runMBProcess_ = true;
      break;
    case '?':
      errflg++;
    }

  if (errflg) {
    fprintf(stderr, "usage: %s\n", usageMessage_);
    fprintf(stderr, "\nProgram <%s> Terminated\n", programName_);
    exit(MB_ERROR_BAD_USAGE);
  }

  if (verbose_ == 1 || help) {
    fprintf(stderr, "\nProgram %s\n", programName_);
    fprintf(stderr, "MB-system Version %s\n", MB_VERSION);
  }

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  Program <%s>\n", programName_);
    fprintf(stderr, "dbg2  MB-system Version %s\n", MB_VERSION);
    fprintf(stderr, "dbg2  Control Parameters:\n");
    fprintf(stderr, "dbg2       verbose:         %d\n", verbose_);
    fprintf(stderr, "dbg2       help:            %d\n", help);
    fprintf(stderr, "dbg2       format:          %d\n", format_);
    fprintf(stderr, "dbg2       input file:      %s\n", ifile_);
  }

  if (help) {
    fprintf(stderr, "\n%s\n", helpMessage_);
    fprintf(stderr, "\nusage: %s\n", usageMessage_);
    exit(error_);
  }

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       argc:      %d\n", argc);
    for (int i = 0; i < argc; i++)
      fprintf(stderr, "dbg2       argv[%d]:    %s\n", i, argv[i]);
  }

  /* if file specified then use it */
  *startup_file = (fileflag > 0);

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}

/*--------------------------------------------------------------------*/
int Backend::set_graphics(void *xgid, int ncol) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       xgid:         %p\n", xgid);
    fprintf(stderr, "dbg2       ncolors:      %d\n", ncol);
  }

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_open(bool useprevious) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  /* clear the screen */
  int status = clear_screen();

  /* open the file */
  status = open_file(useprevious);

  /* load the buffer */
  if (status == MB_SUCCESS)
    status = load_data();

  /* set up plotting */
  if (nBuff_ > 0) {
    /* set time span to zero so plotting resets it */
    dataShowSize_ = 0;

    /* turn file button off */
    disableFileInput();

    /* now plot it */
    status = plot_all();
  }

  /* if no data read show error dialog */
  else
    showError("No data were read from the input",
	      "file. You may have specified an",
	      "incorrect MB-System format id!");

  /* reset data_save */
  dataSave_ = false;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  File open attempted in MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Buffer values:\n");
    fprintf(stderr, "dbg2       nload:       %d\n", nDump_);
    fprintf(stderr, "dbg2       nload:       %d\n", nLoad_);
    fprintf(stderr, "dbg2       nbuff:       %d\n", nBuff_);
    fprintf(stderr, "dbg2       current_id:  %d\n", currentId_);
    fprintf(stderr, "dbg2       error:       %d\n", error_);
  }

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::open_file(bool useprevious) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       file:        %s\n", ifile_);
    fprintf(stderr, "dbg2       format:      %d\n", format_);
    fprintf(stderr, "dbg2       useprevious: %d\n", useprevious);
  }

  mb_pathplusplus ifile_use;
  mb_command command;
  int format_use;
  int form;
  int format_error;
  struct stat file_status;
  int fstat;
  mb_path error1 = "";
  char error2[2100] = "";
  char error3[2100] = "";

  /* swath file locking variables */
  bool locked = false;
  int lock_purpose = MBP_LOCK_NONE;
  mb_path lock_program;
  mb_path lock_cpu;
  mb_path lock_user;
  char lock_date[25];

  /* reset message */
  showMessage("MBedit is opening a data file...");

  /* get format if required */
  if (format_ == 0) {
    if (mb_get_format(verbose_, ifile_, NULL, &form, &format_error) == MB_SUCCESS) {
      format_ = form;
    }
  }

  /* get the output filename */
  strcpy(nfile_, ifile_);
  strcat(nfile_, ".nve");

  int status = MB_SUCCESS;

  /* try to lock file */
  if (outputMode_ == OUTPUT_MODE_OUTPUT && useLockFiles_) {
    status =
      mb_pr_lockswathfile(verbose_, ifile_, MBP_LOCK_EDITNAV, programName_,
			  &error_);
  }
  else {
    // const int lock_status =
    mb_pr_lockinfo(verbose_, ifile_, &locked, &lock_purpose, lock_program,
		   lock_user, lock_cpu, lock_date, &error_);

    /* if locked get lock info */
    if (error_ == MB_ERROR_FILE_LOCKED) {
      fprintf(stderr, "\nFile %s locked but lock ignored\n", ifile_);
      fprintf(stderr, "File locked by <%s> running <%s>\n", lock_user, lock_program);
      fprintf(stderr, "on cpu <%s> at <%s>\n", lock_cpu, lock_date);
      error_ = MB_ERROR_NO_ERROR;
    }
  }

  /* if locked let the user know file can't be opened */
  if (status == MB_FAILURE) {
    /* turn off message */
    hideMessage();

    /* if locked get lock info */
    if (error_ == MB_ERROR_FILE_LOCKED) {
      // const int lock_status =
      mb_pr_lockinfo(verbose_, ifile_, &locked, &lock_purpose, lock_program, lock_user, lock_cpu, lock_date, &error_);

      sprintf(error1, "Unable to open input file:");
      sprintf(error2, "File locked by <%s> running <%s>", lock_user, lock_program);
      sprintf(error3, "on cpu <%s> at <%s>", lock_cpu, lock_date);
      fprintf(stderr, "\nUnable to open input file:\n");
      fprintf(stderr, "  %s\n", ifile_);
      fprintf(stderr, "File locked by <%s> running <%s>\n", lock_user, lock_program);
      fprintf(stderr, "on cpu <%s> at <%s>\n", lock_cpu, lock_date);
    }

    /* else if unable to create lock file there is a permissions problem */
    else if (error_ == MB_ERROR_OPEN_FAIL) {
      sprintf(error1, "Unable to create lock file");
      sprintf(error2, "for intended input file:");
      sprintf(error3, "-Likely permissions issue");
      fprintf(stderr, "Unable to create lock file\n");
      fprintf(stderr, "for intended input file:\n");
      fprintf(stderr, "  %s\n", ifile_);
      fprintf(stderr, "-Likely permissions issue\n");
    }

    /* put up error dialog */
    showError(error1, error2, error3);
  }

  /* if successfully locked (or lock ignored) proceed */
  if (status == MB_SUCCESS) {
    /* if output on and using previously edited nav first copy old nav
       and then read it as input instead of specified
       input file */
    if (useprevious && outputMode_ != OUTPUT_MODE_BROWSE) {
      /* get temporary file name */
      snprintf(ifile_use, sizeof(ifile_use), "%s.tmp", nfile_);

      /* copy old edit save file to tmp file */
      sprintf(command, "cp %s %s\n", nfile_, ifile_use);
      format_use = MBF_MBPRONAV;
      /* const int shellstatus = */ system(command);
      fstat = stat(ifile_use, &file_status);
      if (fstat != 0 || (file_status.st_mode & S_IFMT) == S_IFDIR) {
	showError("Unable to copy previously edited",
		  "navigation. You may not have read",
		  "permission in this directory!");
	
	status = MB_FAILURE;
	return (status);
      }
    }

    /* if output off and using previously edited nav
       reset input names */
    else if (useprevious) {
      snprintf(ifile_use, sizeof(ifile_use), "%s", nfile_);
      format_use = MBF_MBPRONAV;
    }

    /* else just read from previously edited nav */
    else {
      strcpy(ifile_use, ifile_);
      format_use = format_;
    }

    /* initialize reading the input multibeam file */
    status = mb_format_source(verbose_, &format_use, &platformSource_,
			      &navSource_, &sensorDepthSource_,
			      &headingSource_,
			      &attitudeSource_, &svpSource_, &error_);

    if ((status = mb_read_init(verbose_, ifile_use, format_use, nPings_,
			       lonFlip_, bounds_, btime_i_, etime_i_,
			       speedMin_, timeGap_,
			       &imbioPtr_, &btime_d_, &etime_d_,
			       &beamsBath_, &beamsAmp_, &pixelsSS_,
			       &error_)) != MB_SUCCESS) {
      
      mb_error(verbose_, error_, &message_);
      fprintf(stderr, "\nMBIO Error returned from function <mb_read_init>:\n%s\n", message_);
      fprintf(stderr, "\nMultibeam File <%s> not initialized for reading\n", ifile_);
      status = MB_FAILURE;
      showError("Unable to open input file.",
		"You may not have read",
		"permission in this directory!");
      
      return (status);
    }

    /* allocate memory for data arrays */
    beamFlag_ = NULL;
    bath_ = NULL;
    amp_ = NULL;
    bathAcrossTrack_ = NULL;
    bathAlongTrack_ = NULL;
    ss_ = NULL;
    ssAcrossTrack_ = NULL;
    ssAlongTrack_ = NULL;
    if (error_ == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose_, imbioPtr_, MB_MEM_TYPE_BATHYMETRY,
				 sizeof(char), (void **)&beamFlag_, &error_);
    if (error_ == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose_, imbioPtr_, MB_MEM_TYPE_BATHYMETRY,
				 sizeof(double), (void **)&bath_, &error_);
    if (error_ == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose_, imbioPtr_, MB_MEM_TYPE_AMPLITUDE,
				 sizeof(double), (void **)&amp_, &error_);
    if (error_ == MB_ERROR_NO_ERROR)
      status =
	mb_register_array(verbose_, imbioPtr_, MB_MEM_TYPE_BATHYMETRY,
			  sizeof(double), (void **)&bathAcrossTrack_, &error_);
    if (error_ == MB_ERROR_NO_ERROR)
      status =
	mb_register_array(verbose_, imbioPtr_, MB_MEM_TYPE_BATHYMETRY,
			  sizeof(double), (void **)&bathAlongTrack_, &error_);
    if (error_ == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose_, imbioPtr_, MB_MEM_TYPE_SIDESCAN,
				 sizeof(double), (void **)&ss_, &error_);
    if (error_ == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose_, imbioPtr_, MB_MEM_TYPE_SIDESCAN,
				 sizeof(double), (void **)&ssAcrossTrack_,
				 &error_);
    if (error_ == MB_ERROR_NO_ERROR)
      status = mb_register_array(verbose_, imbioPtr_, MB_MEM_TYPE_SIDESCAN,
				 sizeof(double), (void **)&ssAlongTrack_,
				 &error_);

    /* if error initializing memory then quit */
    if (error_ != MB_ERROR_NO_ERROR) {
      mb_error(verbose_, error_, &message_);
      fprintf(stderr, "\nMBIO Error allocating data arrays:\n%s\n", message_);
      fprintf(stderr, "\nProgram <%s> Terminated\n", programName_);
      exit(error_);
    }

    /* initialize the buffer */
    nBuff_ = 0;
    firstRead_ = false;

    /* reset plotting time span */
    plotStartTime_ = 0.0;
    plotEndTime_ = dataShowSize_;

    /* now deal with new nav save file */
    nfileOpen_ = false;
    if (status == MB_SUCCESS && outputMode_ != OUTPUT_MODE_BROWSE) {
      /* get nav edit save file */
      snprintf(nfile_, sizeof(nfile_), "%s.nve", ifile_);

      /* open the nav edit save file */
      if ((nfp_ = fopen(nfile_, "w")) != NULL) {
	nfileOpen_ = true;
      }
      else {
	nfileOpen_ = false;
	fprintf(stderr, "\nUnable to open new nav save file %s\n", nfile_);
	showError("Unable to open new nav edit save file.",
		  "You may not have write",
		  "permission in this directory!");
      }
    }

    /* if we got here we must have succeeded */
    if (verbose_ >= 1) {
      if (useprevious) {
	fprintf(stderr, "\nSwath data file <%s> specified for input\n", ifile_);
	fprintf(stderr, "MB-System Data Format ID: %d\n", format_);
	fprintf(stderr, "Navigation data file <%s> initialized for reading\n", ifile_use);
	fprintf(stderr, "MB-System Data Format ID: %d\n", format_use);
      }
      else {
	fprintf(stderr, "\nSwath data file <%s> initialized for reading\n", ifile_use);
	fprintf(stderr, "MB-System Data Format ID: %d\n", format_use);
      }
      if (outputMode_ == OUTPUT_MODE_OUTPUT)
	fprintf(stderr, "Navigation File <%s> initialized for writing\n", nfile_);
    }
    fileOpen_ = true;
  }

  /* turn off message */
  hideMessage();

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:     %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::close_file(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  /* reset message */
  showMessage("MBedit is closing a data file...");

  /* close the files */
  int status = mb_close(verbose_, &imbioPtr_, &error_);
  if (nfileOpen_) {
    /* close navigation file */
    fclose(nfp_);
    nfileOpen_ = false;
  }

  /* if not in browse mode, deal with locking and processing */
  if (outputMode_ == OUTPUT_MODE_OUTPUT) {

    /* unlock the raw swath file */
    if (useLockFiles_)
      status = mb_pr_unlockswathfile(verbose_, ifile_, MBP_LOCK_EDITNAV,
				     programName_, &error_);

    /* update mbprocess parameter file */
    status = mb_pr_update_format(verbose_, ifile_, true, format_, &error_);
    status = mb_pr_update_nav(verbose_, ifile_, MBP_NAV_ON, nfile_, 9,
			      MBP_NAV_ON, MBP_NAV_ON, MBP_NAV_ON, MBP_NAV_ON,
			      MBP_NAV_LINEAR, (double)0.0, &error_);

    /* run mbprocess if desired */
    if (runMBProcess_) {
      /* turn message on */
      showMessage("Navigation edits being applied using mbprocess...");

      /* run mbprocess */
      char command[MB_PATH_MAXLINE+100];
      if (stripComments_)
	sprintf(command, "mbprocess -I %s -N\n", ifile_);
      else
	sprintf(command, "mbprocess -I %s\n", ifile_);
      if (verbose_ >= 1)
	fprintf(stderr, "\nExecuting command:\n%s\n", command);
      /* const int shellstatus = */ system(command);

      /* turn message off */
      hideMessage();
    }
  }

  /* check memory */
  if (verbose_ >= 4)
    status = mb_memory_list(verbose_, &error_);

  /* if we got here we must have succeeded */
  if (verbose_ >= 1) {
    fprintf(stderr, "\nMultibeam Input File <%s> closed\n", ifile_);
    if (outputMode_ == OUTPUT_MODE_OUTPUT)
      fprintf(stderr, "Navigation Output File <%s> closed\n", nfile_);
    fprintf(stderr, "%d data records loaded\n", nLoadTotal_);
    fprintf(stderr, "%d data records dumped\n", nDumpTotal_);
  }
  fileOpen_ = false;
  nLoadTotal_ = 0;
  nDumpTotal_ = 0;

  /* reset offsets */
  offsetLon_ = 0.0;
  offsetLat_ = 0.0;
  offsetLonApplied_ = offsetLon_;
  offsetLatApplied_ = offsetLat_;

  /* turn file button on */
  enableFileInput();

  /* turn off message */
  hideMessage();

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::dump_data(int hold) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       hold:       %d\n", hold);
  }

  /* write out edited data */
  if (nfileOpen_) {
    for (int iping = 0; iping < nBuff_ - hold; iping++) {
      /* write the nav out */
      fprintf(nfp_, "%4.4d %2.2d %2.2d %2.2d %2.2d %2.2d.%6.6d %16.6f %.10f %.10f %.3f %.3f %.4f %.3f %.3f %.4f\r\n",
	      ping[iping].time_i[0], ping[iping].time_i[1], ping[iping].time_i[2], ping[iping].time_i[3],
	      ping[iping].time_i[4], ping[iping].time_i[5], ping[iping].time_i[6], ping[iping].time_d, ping[iping].lon,
	      ping[iping].lat, ping[iping].heading, ping[iping].speed, ping[iping].draft, ping[iping].roll,
	      ping[iping].pitch, ping[iping].heave);
    }
  }

  /* dump or clear data from the buffer */
  nDump_ = 0;
  if (nBuff_ > 0) {
    /* turn message on */
    showMessage("MBnavedit is clearing data...");

    /* copy data to be held */
    for (int iping = 0; iping < hold; iping++) {
      ping[iping] = ping[iping + nBuff_ - hold];
    }
    nDump_ = nBuff_ - hold;
    nBuff_ = hold;

    /* turn message off */
    hideMessage();
  }
  nDumpTotal_ += nDump_;

  /* reset current data pointer */
  if (nDump_ > 0)
    currentId_ = currentId_ - nDump_;
  if (currentId_ < 0)
    currentId_ = 0;
  if (currentId_ > nBuff_ - 1)
    currentId_ = nBuff_ - 1;

  /* print out information */
  if (verbose_ >= 1) {
    if (outputMode_ == OUTPUT_MODE_OUTPUT)
      fprintf(stderr, "\n%d data records dumped to output file <%s>\n", nDump_, nfile_);
    else
      fprintf(stderr, "\n%d data records dumped from buffer\n", nDump_);
    fprintf(stderr, "%d data records remain in buffer\n", nBuff_);
  }

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::load_data(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  /* turn message on */
  nLoad_ = 0;
  timestampProblem_ = false;
  char string[MB_PATH_MAXLINE];
  sprintf(string, "MBnavedit: %d records loaded so far...", nLoad_);
  showMessage(string);

  /* load data */
  int status = MB_SUCCESS;
  if (status == MB_SUCCESS)
    do {
      status = mb_get_all(verbose_, imbioPtr_, &storePtr_, &kind_,
			  ping[nBuff_].time_i, &ping[nBuff_].time_d,
			  &ping[nBuff_].lon,
			  &ping[nBuff_].lat, &ping[nBuff_].speed,
			  &ping[nBuff_].heading, &distance_, &altitude_,
			  &sensorDepth_,
			  &nbath_, &namp_, &nss_, beamFlag_, bath_, amp_,
			  bathAcrossTrack_, bathAlongTrack_,
			  ss_, ssAcrossTrack_, ssAlongTrack_, comment_,
			  &error_);
      
      if (error_ <= MB_ERROR_NO_ERROR &&
	  (kind_ == navSource_ || (kind_ == MB_DATA_DATA && usePingData_)) &&
	  (error_ == MB_ERROR_NO_ERROR || error_ == MB_ERROR_TIME_GAP
	   || error_ == MB_ERROR_OUT_BOUNDS ||
	   error_ == MB_ERROR_OUT_TIME || error_ == MB_ERROR_SPEED_TOO_SMALL)) {
	status = MB_SUCCESS;
	error_ = MB_ERROR_NO_ERROR;
      }
      else if (error_<= MB_ERROR_NO_ERROR) {
	status = MB_FAILURE;
	error_ = MB_ERROR_OTHER;
      }
      if (error_ == MB_ERROR_NO_ERROR &&
	  (kind_ == navSource_ ||
	   (kind_ == MB_DATA_DATA && usePingData_))) {
	status = mb_extract_nav(verbose_, imbioPtr_, storePtr_, &kind_,
				ping[nBuff_].time_i, &ping[nBuff_].time_d,
				&ping[nBuff_].lon, &ping[nBuff_].lat,
				&ping[nBuff_].speed, &ping[nBuff_].heading,
				&ping[nBuff_].draft, &ping[nBuff_].roll,
				&ping[nBuff_].pitch, &ping[nBuff_].heave,
				&error_);
      }
      if (status == MB_SUCCESS) {
	/* get first time value if first record */
	if (!firstRead_) {
	  fileStarttime_d_ = ping[nBuff_].time_d;
	  firstRead_ = true;
	}

	/* get original values */
	ping[nBuff_].id = nLoad_;
	ping[nBuff_].record = ping[nBuff_].id + nDumpTotal_;
	ping[nBuff_].lon_org = ping[nBuff_].lon;
	ping[nBuff_].lat_org = ping[nBuff_].lat;
	ping[nBuff_].speed_org = ping[nBuff_].speed;
	ping[nBuff_].heading_org = ping[nBuff_].heading;
	ping[nBuff_].draft_org = ping[nBuff_].draft;
	ping[nBuff_].file_time_d = ping[nBuff_].time_d - fileStarttime_d_;

	/* apply offsets */
	ping[nBuff_].lon += offsetLon_;
	ping[nBuff_].lat += offsetLat_;

	/* set starting dr */
	ping[nBuff_].mean_ok = false;
	ping[nBuff_].lon_dr = ping[nBuff_].lon;
	ping[nBuff_].lat_dr = ping[nBuff_].lat;

	/* set everything deselected */
	ping[nBuff_].tint_select = false;
	ping[nBuff_].lon_select = false;
	ping[nBuff_].lat_select = false;
	ping[nBuff_].speed_select = false;
	ping[nBuff_].heading_select = false;
	ping[nBuff_].draft_select = false;
	ping[nBuff_].lonlat_flag = false;

	/* select repeated data */
	if (nBuff_ > 0 && ping[nBuff_].lon == ping[nBuff_ - 1].lon && ping[nBuff_].lat == ping[nBuff_ - 1].lat) {
	  ping[nBuff_].lonlat_flag = true;
	}

	if (verbose_ >= 5) {
	  fprintf(stderr, "\ndbg5  Next good data found in function <%s>:\n", __func__);
	  fprintf(stderr,
		  "dbg5       %4d %4d %4d  %d/%d/%d %2.2d:%2.2d:%2.2d.%6.6d  %15.10f %15.10f %6.3f %7.3f %8.4f %6.3f "
		  "%6.3f %8.4f\n",
		  nBuff_, ping[nBuff_].id, ping[nBuff_].record, ping[nBuff_].time_i[1], ping[nBuff_].time_i[2],
		  ping[nBuff_].time_i[0], ping[nBuff_].time_i[3], ping[nBuff_].time_i[4], ping[nBuff_].time_i[5],
		  ping[nBuff_].time_i[6], ping[nBuff_].lon, ping[nBuff_].lat, ping[nBuff_].speed, ping[nBuff_].heading,
		  ping[nBuff_].draft, ping[nBuff_].roll, ping[nBuff_].pitch, ping[nBuff_].heave);
	}

	/* increment counting variables */
	nBuff_++;
	nLoad_++;

	/* update message every 250 records */
	if (nLoad_ % 250 == 0) {
	  sprintf(string, "MBnavedit: %d records loaded so far...", nLoad_);
	  showMessage(string);
	}
      }
    } while (error_ <= MB_ERROR_NO_ERROR && nBuff_ < MBNAVEDIT_BUFFER_SIZE);
  nLoadTotal_ += nLoad_;

  /* define success */
  if (nBuff_ > 0) {
    status = MB_SUCCESS;
    error_= MB_ERROR_NO_ERROR;
  }

  /* check for time stamp repeats */
  timestampProblem_ = false;
  for (int i = 0; i < nBuff_ - 1; i++) {
    if (ping[i + 1].time_d <= ping[i].time_d) {
      timestampProblem_ = true;
    }
  }

  /* calculate expected time */
  if (nBuff_ > 1) {
    for (int i = 1; i < nBuff_; i++) {
      ping[i].tint = ping[i].time_d - ping[i - 1].time_d;
      ping[i].tint_org = ping[i].tint;
      ping[i].time_d_org = ping[i].time_d;
    }
    ping[0].tint = ping[1].tint;
    ping[0].tint_org = ping[1].tint_org;
    ping[0].time_d_org = ping[0].time_d;
  }
  else if (nBuff_ == 0) {
    ping[0].tint = 0.0;
    ping[0].tint_org = 0.0;
    ping[0].time_d_org = ping[0].time_d;
  }

  /* find index of current ping */
  currentId_ = 0;

  /* reset plotting time span */
  if (nBuff_ > 0) {
    dataShowSize_ = 0;
    plotStartTime_ = ping[0].file_time_d;
    plotEndTime_ = ping[nBuff_ - 1].file_time_d;
    nPlot_ = nBuff_;
  }

  /* calculate speed-made-good and course-made-good */
  for (int i = 0; i < nBuff_; i++)
    get_smgcmg(i);

  /* calculate model */
  get_model();

  /* turn message off */
  hideMessage();

  /* print out information */
  if (verbose_ >= 1) {
    fprintf(stderr, "\n%d data records loaded from input file <%s>\n", nLoad_, ifile_);
    fprintf(stderr, "%d data records now in buffer\n", nBuff_);
    fprintf(stderr, "Current data record:        %d\n", currentId_);
    fprintf(stderr, "Current global data record: %d\n", currentId_ + nDumpTotal_);
  }

  /* put up warning if timestamp problem detected */
  if (timestampProblem_) {
    showError("Duplicate or reverse order time",
	      "stamps detected!! Time interpolation",
	      "available under Controls menu.");
  }

  /* update controls */
  (*setUiElements)();

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::clear_screen(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  /* clear screen */
  PixmapDrawer::fillRectangle(painter_, 0, 0, plotWidth_,
			      NUMBER_PLOTS_MAX * plotHeight_, WHITE,
			      SOLID_LINE);

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_next_buffer(bool *quit) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  /* clear the screen */
  int status = clear_screen();

  /* set quit off */
  *quit = false;

  /* check if a file has been opened */
  if (fileOpen_) {

    /* dump the buffer */
    status = dump_data(holdSize_);

    /* load the buffer */
    status = load_data();

    /* if end of file reached then
       dump last buffer and close file */
    if (nLoad_ <= 0) {
      const int save_dumped = nDump_;
      status = dump_data(0);
      status = close_file();
      nDump_ = nDump_ + save_dumped;

      /* if in normal mode last next_buffer
	 does not mean quit,
	 if in gui mode it does mean quit */
      if (guiMode_)
	*quit = true;
      else
	*quit = false;

      /* if quitting let the world know... */
      if (*quit && verbose_ >= 1)
	fprintf(stderr, "\nQuitting MBnavedit\nBye Bye...\n");
    }

    /* else plot it */
    else {
      status = plot_all();
    }
  }

  /* if no file open set failure status */
  else {
    status = MB_FAILURE;
    nDump_ = 0;
    nLoad_ = 0;
    currentId_ = 0;
  }

  /* reset data_save */
  dataSave_ = false;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       quit:        %d\n", *quit);
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_offset(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  /* check if a file has been opened */
  if (fileOpen_) {
    /* apply position offsets to the data */
    for (int i = 0; i < nBuff_; i++) {
      ping[i].lon += offsetLon_ - offsetLonApplied_;
      ping[i].lat += offsetLat_ - offsetLatApplied_;
    }
  }
  offsetLonApplied_ = offsetLon_;
  offsetLatApplied_ = offsetLat_;

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_close(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  int status = clear_screen();

  /* if file has been opened and browse mode
     just dump the current buffer and close the file */
  if (fileOpen_ && outputMode_ == OUTPUT_MODE_BROWSE) {
    /* dump the buffer */
    status = dump_data(0);

    /* now close the file */
    status = close_file();
  }
  /* if file has been opened deal with it */
  else if (fileOpen_) {
    /* dump and load until the end of the file is reached */
    int save_ndumped = 0;
    int save_nloaded = 0;
    do {
      /* dump the buffer */
      status = dump_data(0);
      save_ndumped += nDump_;

      /* load the buffer */
      status = load_data();
      save_nloaded += nLoad_;
    } while (nLoad_ > 0);
    nDump_ = save_ndumped;
    nLoad_ = save_nloaded;

    /* now close the file */
    status = close_file();
  }
  else {
    nDump_ = 0;
    nLoad_ = 0;
    nBuff_ = 0;
    currentId_ = 0;
    status = MB_FAILURE;
  }

  /* reset data_save */
  dataSave_ = false;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_done(bool *quit) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  /* if in normal mode done does not mean quit,
     if in gui mode done does mean quit */
  if (guiMode_)
    *quit = true;
  else
    *quit = false;

  /* if quitting let the world know... */
  if (*quit && verbose_ >= 1)
    fprintf(stderr, "\nShutting MBnavedit down without further ado...\n");

  /* call routine to deal with saving the current file, if any */
  int status = MB_SUCCESS;
  if (fileOpen_)
    status = action_close();

  /* if quitting let the world know... */
  if (*quit && verbose_ >= 1)
    fprintf(stderr, "\nQuitting MBnavedit\nBye Bye...\n");

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       quit:        %d\n", *quit);
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_quit(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  /* let the world know... */
  if (verbose_ >= 1)
    fprintf(stderr, "\nShutting MBnavedit down without further ado...\n");

  /* call routine to deal with saving the current file, if any */
  int status = MB_SUCCESS;
  if (fileOpen_)
    status = action_close();

  /* let the world know... */
  if (verbose_ >= 1)
    fprintf(stderr, "\nQuitting MBnavedit\nBye Bye...\n");

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_step(int step) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       step:       %d\n", step);
  }

  int status = MB_SUCCESS;

  /* check if a file has been opened */
  if (fileOpen_ && nBuff_ > 0) {

    /* if current time span includes last data don't step */
    if (step >= 0 && plotEndTime_ < ping[nBuff_ - 1].file_time_d) {
      plotStartTime_ = plotStartTime_ + step;
      plotEndTime_ = plotStartTime_ + dataShowSize_;
    }
    else if (step < 0 && plotStartTime_ > ping[0].file_time_d) {
      plotStartTime_ = plotStartTime_ + step;
      plotEndTime_ = plotStartTime_ + dataShowSize_;
    }

    /* get current start of plotting data */
    bool set = false;
    const int old_id = currentId_;
    int new_id;
    for (int i = 0; i < nBuff_; i++) {
      if (!set && ping[i].file_time_d >= plotStartTime_) {
	new_id = i;
	set = true;
      }
    }
    if (new_id < 0)
      new_id = 0;
    if (new_id >= nBuff_)
      new_id = nBuff_ - 1;
    if (step < 0 && new_id > 0 && new_id == old_id)
      new_id--;
    if (step > 0 && new_id < nBuff_ - 1 && new_id == old_id)
      new_id++;
    currentId_ = new_id;

    /* replot */
    if (nBuff_ > 0) {
      status = plot_all();
    }

    /* set failure flag if no step was made */
    if (new_id == old_id)
      status = MB_FAILURE;
  }

  /* if no file open set failure status */
  else {
    status = MB_FAILURE;
    currentId_ = 0;
  }

  /* print out information */
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  Current buffer values:\n");
    fprintf(stderr, "dbg2       nload:       %d\n", nLoad_);
    fprintf(stderr, "dbg2       nbuff:       %d\n", nBuff_);
    fprintf(stderr, "dbg2       nbuff:       %d\n", nBuff_);
    fprintf(stderr, "dbg2       nbuff:       %d\n", nBuff_);
    fprintf(stderr, "dbg2       current_id:  %d\n", currentId_);
  }

  /* reset data_save */
  dataSave_ = false;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_end(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  int status = MB_SUCCESS;

  /* check if a file has been opened */
  if (fileOpen_ && nBuff_ > 0) {
    /* set time span to include last data */
    plotEndTime_ = ping[nBuff_ - 1].file_time_d;
    plotStartTime_ = plotEndTime_ - dataShowSize_;

    /* get current start of plotting data */
    const int old_id = currentId_;
    bool set = false;
    for (int i = 0; i < nBuff_ && !set; i++) {
      if (ping[i].file_time_d >= plotStartTime_) {
	currentId_ = i;
	set = true;
      }
    }

    /* replot */
    status = plot_all();

    /* set failure flag if no step was made */
    if (currentId_ == old_id)
      status = MB_FAILURE;
  }

  /* if no file open set failure status */
  else {
    status = MB_FAILURE;
    currentId_ = 0;
  }

  /* print out information */
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  Current buffer values:\n");
    fprintf(stderr, "dbg2       nload:       %d\n", nLoad_);
    fprintf(stderr, "dbg2       nbuff:       %d\n", nBuff_);
    fprintf(stderr, "dbg2       nbuff:       %d\n", nBuff_);
    fprintf(stderr, "dbg2       nbuff:       %d\n", nBuff_);
    fprintf(stderr, "dbg2       current_id:  %d\n", currentId_);
  }

  /* reset data_save */
  dataSave_ = false;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_start(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  int status = MB_SUCCESS;

  /* check if a file has been opened */
  if (fileOpen_ && nBuff_ > 0) {
    const int old_id = currentId_;
    currentId_ = 0;
    plotStartTime_ = ping[currentId_].file_time_d;
    plotEndTime_ = plotStartTime_ + dataShowSize_;

    /* replot */
    if (nBuff_ > 0) {
      status = plot_all();
    }

    /* set failure flag if no step was made */
    if (currentId_ == old_id)
      status = MB_FAILURE;
  }

  /* if no file open set failure status */
  else {
    status = MB_FAILURE;
    currentId_ = 0;
  }

  /* print out information */
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  Current buffer values:\n");
    fprintf(stderr, "dbg2       nload:       %d\n", nLoad_);
    fprintf(stderr, "dbg2       nbuff:       %d\n", nBuff_);
    fprintf(stderr, "dbg2       nbuff:       %d\n", nBuff_);
    fprintf(stderr, "dbg2       nbuff:       %d\n", nBuff_);
    fprintf(stderr, "dbg2       current_id:  %d\n", currentId_);
  }

  /* reset data_save */
  dataSave_ = false;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_mouse_pick(int xx, int yy) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       xx:         %d\n", xx);
    fprintf(stderr, "dbg2       yy:         %d\n", yy);
  }

  /* don't try to do anything if no data */
  int active_plot = -1;
  if (nPlot_ > 0) {
    /* figure out which plot the cursor is in */
    for (int iplot = 0; iplot < nPlots_; iplot++) {
      if (xx >= plot_[iplot].ixmin && xx <= plot_[iplot].ixmax && yy <= plot_[iplot].iymin &&
	  yy >= plot_[iplot].iymax)
	active_plot = iplot;
    }
  }

  int status = MB_SUCCESS;

  /* don't try to do anything if no data or not in plot */
  if (nPlot_ > 0 && active_plot > -1) {
    /* deselect everything in non-active plots */
    bool deselect = false;
    for (int iplot = 0; iplot < nPlots_; iplot++) {
      if (iplot != active_plot) {
	status = action_deselect_all(plot_[iplot].type);
	if (status == MB_SUCCESS)
	  deselect = true;
      }
    }

    /* if anything was actually deselected, replot */
    if (deselect == MB_SUCCESS) {
      /* clear the screen */
      status = clear_screen();

      /* replot the screen */
      status = plot_all();
    }
    status = MB_SUCCESS;

    /* figure out which data point is closest to cursor */
    int range_min = 100000;
    int iping;
    int ix;
    int iy;
    for (int i = currentId_ + 1; i < currentId_ + nPlot_; i++) {
      // TODO(schwehr): Why not a switch?
      if (plot_[active_plot].type == PLOT_TINT) {
	ix = xx - ping[i].tint_x;
	iy = yy - ping[i].tint_y;
      }
      else if (plot_[active_plot].type == PLOT_LONGITUDE) {
	ix = xx - ping[i].lon_x;
	iy = yy - ping[i].lon_y;
      }
      else if (plot_[active_plot].type == PLOT_LATITUDE) {
	ix = xx - ping[i].lat_x;
	iy = yy - ping[i].lat_y;
      }
      else if (plot_[active_plot].type == PLOT_SPEED) {
	ix = xx - ping[i].speed_x;
	iy = yy - ping[i].speed_y;
      }
      else if (plot_[active_plot].type == PLOT_HEADING) {
	ix = xx - ping[i].heading_x;
	iy = yy - ping[i].heading_y;
      }
      else if (plot_[active_plot].type == PLOT_DRAFT) {
	ix = xx - ping[i].draft_x;
	iy = yy - ping[i].draft_y;
      }
      // TODO(schwehr): What about else?  It might get a prior value.
      const int range = (int)sqrt((double)(ix * ix + iy * iy));
      if (range < range_min) {
	range_min = range;
	iping = i;
      }
    }

    /* if it is close enough select or unselect the value
       and replot it */
    if (range_min <= MBNAVEDIT_PICK_DISTANCE) {
      if (plot_[active_plot].type == PLOT_TINT) {
	if (ping[iping].tint_select)
	  ping[iping].tint_select = false;
	else
	  ping[iping].tint_select = true;
	plot_tint_value(active_plot, iping);
      }
      else if (plot_[active_plot].type == PLOT_LONGITUDE) {
	if (ping[iping].lon_select)
	  ping[iping].lon_select = false;
	else
	  ping[iping].lon_select = true;
	plot_lon_value(active_plot, iping);
      }
      else if (plot_[active_plot].type == PLOT_LATITUDE) {
	if (ping[iping].lat_select)
	  ping[iping].lat_select = false;
	else
	  ping[iping].lat_select = true;
	plot_lat_value(active_plot, iping);
      }
      else if (plot_[active_plot].type == PLOT_SPEED) {
	if (ping[iping].speed_select)
	  ping[iping].speed_select = false;
	else
	  ping[iping].speed_select = true;
	plot_speed_value(active_plot, iping);
      }
      else if (plot_[active_plot].type == PLOT_HEADING) {
	if (ping[iping].heading_select)
	  ping[iping].heading_select = false;
	else
	  ping[iping].heading_select = true;
	plot_heading_value(active_plot, iping);
      }
      else if (plot_[active_plot].type == PLOT_DRAFT) {
	if (ping[iping].draft_select)
	  ping[iping].draft_select = false;
	else
	  ping[iping].draft_select = true;
	plot_draft_value(active_plot, iping);
      }
    }
  }
  /* if no data then set failure flag */
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_mouse_select(int xx, int yy) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       xx:         %d\n", xx);
    fprintf(stderr, "dbg2       yy:         %d\n", yy);
  }

  /* don't try to do anything if no data */
  int active_plot = -1;
  if (nPlot_ > 0) {

    /* figure out which plot the cursor is in */
    for (int iplot = 0; iplot < nPlots_; iplot++) {
      if (xx >= plot_[iplot].ixmin && xx <= plot_[iplot].ixmax && yy <= plot_[iplot].iymin &&
	  yy >= plot_[iplot].iymax)
	active_plot = iplot;
    }
  }

  int status = MB_SUCCESS;

  /* don't try to do anything if no data or not in plot */
  if (nPlot_ > 0 && active_plot > -1) {

    /* deselect everything in non-active plots */
    bool deselect = false;
    for (int iplot = 0; iplot < nPlots_; iplot++) {
      if (iplot != active_plot) {
	status = action_deselect_all(plot_[iplot].type);
	if (status == MB_SUCCESS)
	  deselect = true;
      }
    }

    /* if anything was actually deselected, replot */
    if (deselect == MB_SUCCESS) {
      /* clear the screen */
      status = clear_screen();

      /* replot the screen */
      status = plot_all();
    }
    status = MB_SUCCESS;

    /* find all data points that are close enough */
    int ix;
    int iy;
    for (int i = currentId_; i < currentId_ + nPlot_; i++) {
      if (plot_[active_plot].type == PLOT_TINT) {
	ix = xx - ping[i].tint_x;
	iy = yy - ping[i].tint_y;
      }
      else if (plot_[active_plot].type == PLOT_LONGITUDE) {
	ix = xx - ping[i].lon_x;
	iy = yy - ping[i].lon_y;
      }
      else if (plot_[active_plot].type == PLOT_LATITUDE) {
	ix = xx - ping[i].lat_x;
	iy = yy - ping[i].lat_y;
      }
      else if (plot_[active_plot].type == PLOT_SPEED) {
	ix = xx - ping[i].speed_x;
	iy = yy - ping[i].speed_y;
      }
      else if (plot_[active_plot].type == PLOT_HEADING) {
	ix = xx - ping[i].heading_x;
	iy = yy - ping[i].heading_y;
      }
      else if (plot_[active_plot].type == PLOT_DRAFT) {
	ix = xx - ping[i].draft_x;
	iy = yy - ping[i].draft_y;
      }
      const int range = (int)sqrt((double)(ix * ix + iy * iy));

      /* if it is close enough select the value
	 and replot it */
      if (range <= MBNAVEDIT_ERASE_DISTANCE) {
	if (plot_[active_plot].type == PLOT_TINT) {
	  ping[i].tint_select = true;
	  plot_tint_value(active_plot, i);
	}
	else if (plot_[active_plot].type == PLOT_LONGITUDE) {
	  ping[i].lon_select = true;
	  plot_lon_value(active_plot, i);
	}
	else if (plot_[active_plot].type == PLOT_LATITUDE) {
	  ping[i].lat_select = true;
	  plot_lat_value(active_plot, i);
	}
	else if (plot_[active_plot].type == PLOT_SPEED) {
	  ping[i].speed_select = true;
	  plot_speed_value(active_plot, i);
	}
	else if (plot_[active_plot].type == PLOT_HEADING) {
	  ping[i].heading_select = true;
	  plot_heading_value(active_plot, i);
	}
	else if (plot_[active_plot].type == PLOT_DRAFT) {
	  ping[i].draft_select = true;
	  plot_draft_value(active_plot, i);
	}
      }
    }
  }
  /* if no data then set failure flag */
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_mouse_deselect(int xx, int yy) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       xx:         %d\n", xx);
    fprintf(stderr, "dbg2       yy:         %d\n", yy);
  }

  /* don't try to do anything if no data */
  int active_plot = -1;
  if (nPlot_ > 0) {
    /* figure out which plot the cursor is in */
    for (int iplot = 0; iplot < nPlots_; iplot++) {
      if (xx >= plot_[iplot].ixmin && xx <= plot_[iplot].ixmax && yy <= plot_[iplot].iymin &&
	  yy >= plot_[iplot].iymax)
	active_plot = iplot;
    }
  }

  int status = MB_SUCCESS;

  /* don't try to do anything if no data or not in plot */
  if (nPlot_ > 0 && active_plot > -1) {
    /* deselect everything in non-active plots */
    bool deselect = false;
    for (int iplot = 0; iplot < nPlots_; iplot++) {
      if (iplot != active_plot) {
	status = action_deselect_all(plot_[iplot].type);
	if (status == MB_SUCCESS)
	  deselect = true;
      }
    }

    /* if anything was actually deselected, replot */
    if (deselect == MB_SUCCESS) {
      /* clear the screen */
      status = clear_screen();

      /* replot the screen */
      status = plot_all();
    }
    status = MB_SUCCESS;

    /* find all data points that are close enough */
    int ix;
    int iy;
    for (int i = currentId_; i < currentId_ + nPlot_; i++) {
      if (plot_[active_plot].type == PLOT_TINT) {
	ix = xx - ping[i].tint_x;
	iy = yy - ping[i].tint_y;
      }
      else if (plot_[active_plot].type == PLOT_LONGITUDE) {
	ix = xx - ping[i].lon_x;
	iy = yy - ping[i].lon_y;
      }
      else if (plot_[active_plot].type == PLOT_LATITUDE) {
	ix = xx - ping[i].lat_x;
	iy = yy - ping[i].lat_y;
      }
      else if (plot_[active_plot].type == PLOT_SPEED) {
	ix = xx - ping[i].speed_x;
	iy = yy - ping[i].speed_y;
      }
      else if (plot_[active_plot].type == PLOT_HEADING) {
	ix = xx - ping[i].heading_x;
	iy = yy - ping[i].heading_y;
      }
      else if (plot_[active_plot].type == PLOT_DRAFT) {
	ix = xx - ping[i].draft_x;
	iy = yy - ping[i].draft_y;
      }
      const int range = (int)sqrt((double)(ix * ix + iy * iy));

      /* if it is close enough deselect the value
	 and replot it */
      if (range <= MBNAVEDIT_ERASE_DISTANCE) {
	if (plot_[active_plot].type == PLOT_TINT) {
	  ping[i].tint_select = false;
	  plot_tint_value(active_plot, i);
	}
	else if (plot_[active_plot].type == PLOT_LONGITUDE) {
	  ping[i].lon_select = false;
	  plot_lon_value(active_plot, i);
	}
	else if (plot_[active_plot].type == PLOT_LATITUDE) {
	  ping[i].lat_select = false;
	  plot_lat_value(active_plot, i);
	}
	else if (plot_[active_plot].type == PLOT_SPEED) {
	  ping[i].speed_select = false;
	  plot_speed_value(active_plot, i);
	}
	else if (plot_[active_plot].type == PLOT_HEADING) {
	  ping[i].heading_select = false;
	  plot_heading_value(active_plot, i);
	}
	else if (plot_[active_plot].type == PLOT_DRAFT) {
	  ping[i].draft_select = false;
	  plot_draft_value(active_plot, i);
	}
      }
    }
  }
  /* if no data then set failure flag */
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_mouse_selectall(int xx, int yy) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       xx:         %d\n", xx);
    fprintf(stderr, "dbg2       yy:         %d\n", yy);
  }

  /* don't try to do anything if no data */
  int active_plot = -1;
  if (nPlot_ > 0) {

    /* figure out which plot the cursor is in */
    for (int iplot = 0; iplot < nPlots_; iplot++) {
      if (xx >= plot_[iplot].ixmin && xx <= plot_[iplot].ixmax && yy <= plot_[iplot].iymin &&
	  yy >= plot_[iplot].iymax)
	active_plot = iplot;
    }
  }

  int status = MB_SUCCESS;

  /* don't try to do anything if no data or not in plot */
  if (nPlot_ > 0 && active_plot > -1) {
    /* deselect everything in non-active plots */
    for (int iplot = 0; iplot < nPlots_; iplot++) {
      if (iplot != active_plot) {
	action_deselect_all(plot_[iplot].type);
      }
    }

    /* select all data points in active plot */
    for (int i = currentId_; i < currentId_ + nPlot_; i++) {
      if (plot_[active_plot].type == PLOT_TINT)
	ping[i].tint_select = true;
      else if (plot_[active_plot].type == PLOT_LONGITUDE)
	ping[i].lon_select = true;
      else if (plot_[active_plot].type == PLOT_LATITUDE)
	ping[i].lat_select = true;
      else if (plot_[active_plot].type == PLOT_SPEED)
	ping[i].speed_select = true;
      else if (plot_[active_plot].type == PLOT_HEADING)
	ping[i].heading_select = true;
      else if (plot_[active_plot].type == PLOT_DRAFT)
	ping[i].draft_select = true;
    }

    /* clear the screen */
    status = clear_screen();

    /* replot the screen */
    status = plot_all();
  }
  /* if no data then set failure flag */
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_mouse_deselectall(int xx, int yy) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       xx:         %d\n", xx);
    fprintf(stderr, "dbg2       yy:         %d\n", yy);
  }

  int status = MB_SUCCESS;

  /* don't try to do anything if no data */
  if (nPlot_ > 0) {

    /* deselect all data points in all plots
       - this logic follows from deselecting all
       active plots plus all non-active plots */
    for (int i = currentId_; i < currentId_ + nPlot_; i++) {
      ping[i].tint_select = false;
      ping[i].lon_select = false;
      ping[i].lat_select = false;
      ping[i].speed_select = false;
      ping[i].heading_select = false;
      ping[i].draft_select = false;
    }

    /* clear the screen */
    status = clear_screen();

    /* replot the screen */
    status = plot_all();
  }
  /* if no data then set failure flag */
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_deselect_all(int type) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       type:       %d\n", type);
  }

  int status = MB_SUCCESS;

  /* don't try to do anything if no data */
  if (nPlot_ > 0) {
    /* deselect all data points in specified data type */
    int ndeselect = 0;
    for (int i = 0; i < nBuff_; i++) {
      if (type == PLOT_TINT && ping[i].tint_select) {
	ping[i].tint_select = false;
	ndeselect++;
      }
      else if (type == PLOT_LONGITUDE && ping[i].lon_select) {
	ping[i].lon_select = false;
	ndeselect++;
      }
      else if (type == PLOT_LATITUDE && ping[i].lat_select) {
	ping[i].lat_select = false;
	ndeselect++;
      }
      else if (type == PLOT_SPEED && ping[i].speed_select) {
	ping[i].speed_select = false;
	ndeselect++;
      }
      else if (type == PLOT_HEADING && ping[i].heading_select) {
	ping[i].heading_select = false;
	ndeselect++;
      }
      else if (type == PLOT_DRAFT && ping[i].draft_select) {
	ping[i].draft_select = false;
	ndeselect++;
      }
    }
    if (ndeselect > 0)
      status = MB_SUCCESS;
    else
      status = MB_FAILURE;
  }
  /* if no data then set failure flag */
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_set_interval(int xx, int yy, int which) {
  // TODO(schwehr): Explain why these need to be static.
  static int interval_bound1;
  static int interval_bound2;
  static double interval_time1;
  static double interval_time2;
  static bool interval_set1 = false;
  static bool interval_set2 = false;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       xx:         %d\n", xx);
    fprintf(stderr, "dbg2       yy:         %d\n", yy);
    fprintf(stderr, "dbg2       which:      %d\n", which);
  }

  int status = MB_SUCCESS;

  /* don't try to do anything if no data */
  if (nPlot_ > 0 && nPlots_ > 0) {
    /* if which = 0 set first bound and draw dashed lines */
    if (which == 0) {
      /* unplot old line on all plots */
      if (interval_set1)
	for (int i = 0; i < nPlots_; i++) {
	  PixmapDrawer::drawLine(painter_,
				 interval_bound1,
				 plot_[i].iymin,
				 interval_bound1,
				 plot_[i].iymax,
				 WHITE,
				 DASH_LINE);
	}

      if (xx < plot_[0].ixmin)
	xx = plot_[0].ixmin;
      if (xx > plot_[0].ixmax)
	xx = plot_[0].ixmax;

      /* get lower bound time and location */
      interval_bound1 = xx;
      interval_time1 = plot_[0].xmin + (xx - plot_[0].ixmin) / plot_[0].xscale;
      interval_set1 = true;

      /* plot line on all plots */
      for (int i = 0; i < nPlots_; i++) {
	PixmapDrawer::drawLine(painter_, interval_bound1,
			       plot_[i].iymin,
			       interval_bound1,
			       plot_[i].iymax,
			       RED, DASH_LINE);
      }
    }

    /* if which = 1 set second bound and draw dashed lines */
    else if (which == 1) {
      /* unplot old line on all plots */
      if (interval_set1)
	for (int i = 0; i < nPlots_; i++) {
	  PixmapDrawer::drawLine(painter_,
				 interval_bound2,
				 plot_[i].iymin,
				 interval_bound2,
				 plot_[i].iymax,
				 WHITE,
				 DASH_LINE);
	}

      if (xx < plot_[0].ixmin)
	xx = plot_[0].ixmin;
      if (xx > plot_[0].ixmax)
	xx = plot_[0].ixmax;

      /* get lower bound time and location */
      interval_bound2 = xx;
      interval_time2 = plot_[0].xmin + (xx - plot_[0].ixmin) / plot_[0].xscale;
      interval_set2 = true;

      /* plot line on all plots */
      for (int i = 0; i < nPlots_; i++) {
	PixmapDrawer::drawLine(painter_, interval_bound2,
			       plot_[i].iymin,
			       interval_bound2, plot_[i].iymax,
			       RED, DASH_LINE);
      }
    }

    /* if which = 2 use bounds and replot */
    else if (which == 2 && interval_set1 && interval_set2 && interval_bound1 != interval_bound2) {
      /* switch bounds if necessary */
      if (interval_bound1 > interval_bound2) {
	const int itmp = interval_bound2;
	const double dtmp = interval_time2;
	interval_bound2 = interval_bound1;
	interval_time2 = interval_time1;
	interval_bound1 = itmp;
	interval_time1 = dtmp;
      }

      /* reset plotting parameters */
      plotStartTime_ = interval_time1;
      plotEndTime_ = interval_time2;
      dataShowSize_ = plotEndTime_ - plotStartTime_;

      /* reset time stepping parameters */
      dataStepSize_ = dataShowSize_ / 4;
      if (dataStepSize_ > dataStepMax_)
	dataStepMax_ = 2 * dataStepSize_;

      /* get current start of plotting data */
      bool set = false;
      for (int i = 0; i < nBuff_; i++) {
	if (!set && ping[i].file_time_d >= plotStartTime_) {
	  currentId_ = i;
	  set = true;
	}
      }
      if (currentId_ < 0)
	currentId_ = 0;
      if (currentId_ >= nBuff_)
	currentId_ = nBuff_ - 1;

      /* replot */
      plot_all();
    }

    /* else if which = 3 unset bounds */
    else if (which == 3) {
      interval_set1 = false;
      interval_set2 = false;
    }

    /* else failure */
    else
      status = MB_FAILURE;
  }
  /* if no data then set failure flag */
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_use_dr(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  int status = MB_SUCCESS;

  /* don't try to do anything if no data */
  if (nPlot_ > 0) {
    /* make sure either a lon or lat plot is active */
    int active_plot = -1;
    for (int iplot = 0; iplot < nPlots_; iplot++) {
      if (plot_[iplot].type == PLOT_LONGITUDE)
	active_plot = iplot;
      else if (plot_[iplot].type == PLOT_LATITUDE)
	active_plot = iplot;
    }

    /* set lonlat to dr lonlat for selected visible data */
    if (active_plot > -1) {
      for (int i = currentId_; i < currentId_ + nPlot_; i++) {
	if (ping[i].lon_select || ping[i].lat_select) {
	  ping[i].lon = ping[i].lon_dr;
	  ping[i].lat = ping[i].lat_dr;
	}
      }

      /* calculate speed-made-good and course-made-good */
      for (int i = 0; i < nBuff_; i++)
	get_smgcmg(i);

      /* clear the screen */
      status = clear_screen();

      /* replot the screen */
      status = plot_all();
    }

    else
      status = MB_FAILURE;
  }
  /* if no data then set failure flag */
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_use_smg(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  int status = MB_SUCCESS;

  /* don't try to do anything if no data */
  if (nPlot_ > 0) {
    /* figure out which plot is speed */
    int active_plot = -1;
    for (int iplot = 0; iplot < nPlots_; iplot++) {
      if (plot_[iplot].type == PLOT_SPEED)
	active_plot = iplot;
    }

    /* set speed to speed made good for selected visible data */
    if (active_plot > -1) {
      bool speedheading_change = false;
      for (int i = currentId_; i < currentId_ + nPlot_; i++) {
	if (ping[i].speed_select) {
	  ping[i].speed = ping[i].speed_made_good;
	  speedheading_change = true;
	}
      }

      /* recalculate model */
      if (speedheading_change && modelMode_ == MODEL_MODE_DR)
	get_model();

      status = clear_screen();

      /* replot the screen */
      status = plot_all();
    }

    else
      status = MB_FAILURE;
  }
  /* if no data then set failure flag */
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_use_cmg(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  int status = MB_SUCCESS;

  /* don't try to do anything if no data */
  if (nPlot_ > 0) {
    /* figure out which plot is heading */
    int active_plot = -1;
    for (int iplot = 0; iplot < nPlots_; iplot++) {
      if (plot_[iplot].type == PLOT_HEADING)
	active_plot = iplot;
    }

    /* set heading to course made good for selected visible data */
    if (active_plot > -1) {
      bool speedheading_change = false;
      for (int i = currentId_; i < currentId_ + nPlot_; i++) {
	if (ping[i].heading_select) {
	  ping[i].heading = ping[i].course_made_good;
	  speedheading_change = true;
	}
      }

      /* recalculate model */
      if (speedheading_change && modelMode_ == MODEL_MODE_DR)
	get_model();

      /* clear the screen */
      status = clear_screen();

      /* replot the screen */
      status = plot_all();
    }

    else
      status = MB_FAILURE;
  }
  /* if no data then set failure flag */
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_interpolate(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  int status = MB_SUCCESS;
  double dtime;

  /* don't try to do anything if no data */
  if (nPlot_ > 0) {
    /* look for position or time changes */
    bool timelonlat_change = false;
    bool speedheading_change = false;

    /* do expected time */
    for (int iping = 0; iping < nBuff_; iping++) {
      if (ping[iping].tint_select) {
	int ibefore = iping;
	for (int i = iping - 1; i >= 0; i--) {
	  if (!ping[i].tint_select && ibefore == iping)
	    ibefore = i;
	}
	int iafter = iping;
	for (int i = iping + 1; i < nBuff_; i++) {
	  if (!ping[i].tint_select && iafter == iping)
	    iafter = i;
	}
	if (ibefore < iping && iafter > iping) {
	  ping[iping].time_d = ping[ibefore].time_d + (ping[iafter].time_d - ping[ibefore].time_d) *
	    ((double)(iping - ibefore)) / ((double)(iafter - ibefore));
	  ping[iping].tint = ping[iping].time_d - ping[iping - 1].time_d;
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
	else if (ibefore < iping && ibefore > 0) {
	  ping[iping].time_d =
	    ping[ibefore].time_d + (ping[ibefore].time_d - ping[ibefore - 1].time_d) * (iping - ibefore);
	  ping[iping].tint = ping[iping].time_d - ping[iping - 1].time_d;
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
	else if (ibefore < iping) {
	  ping[iping].time_d = ping[ibefore].time_d;
	  ping[iping].tint = ping[iping].time_d - ping[iping - 1].time_d;
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
	else if (iafter > iping && iafter < nBuff_ - 1) {
	  ping[iping].time_d = ping[iafter].time_d + (ping[iafter + 1].time_d - ping[iafter].time_d) * (iping - iafter);
	  ping[iping].tint = 0.0;
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
	else if (iafter > iping) {
	  ping[iping].time_d = ping[iafter].time_d;
	  ping[iping].tint = ping[iping].time_d - ping[iping - 1].time_d;
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
	ping[iping].file_time_d = ping[iping].time_d - fileStarttime_d_;
	status = mb_get_date(verbose_, ping[iping].time_d, ping[iping].time_i);
	if (iping < nBuff_ - 1)
	  if (!ping[iping + 1].tint_select)
	    ping[iping + 1].tint = ping[iping + 1].time_d - ping[iping].time_d;
      }
    }

    /* do longitude */
    for (int iping = 0; iping < nBuff_; iping++) {
      if (ping[iping].lon_select) {
	int ibefore = iping;
	for (int i = iping - 1; i >= 0; i--) {
	  if (!ping[i].lon_select && ibefore == iping)
	    ibefore = i;
	}
	int iafter = iping;
	for (int i = iping + 1; i < nBuff_; i++) {
	  if (!ping[i].lon_select && iafter == iping)
	    iafter = i;
	}
	if (ibefore < iping && iafter > iping) {
          dtime = ping[iafter].time_d - ping[ibefore].time_d;
          if (dtime > 0.0)
	    ping[iping].lon = ping[ibefore].lon + (ping[iafter].lon - ping[ibefore].lon) *
	      (ping[iping].time_d - ping[ibefore].time_d) /
	      (ping[iafter].time_d - ping[ibefore].time_d);
          else
	    ping[iping].lon = ping[ibefore].lon + 0.5 * (ping[iafter].lon - ping[ibefore].lon);
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
	else if (ibefore < iping && ibefore > 0) {
          dtime = ping[iafter].time_d - ping[ibefore - 1].time_d;
	  if (dtime > 0.0)
	    ping[iping].lon = ping[ibefore].lon + (ping[ibefore].lon - ping[ibefore - 1].lon) *
	      (ping[iping].time_d - ping[ibefore].time_d) /
	      (ping[ibefore].time_d - ping[ibefore - 1].time_d);
          else
            ping[iping].lon = ping[ibefore].lon;
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
	else if (ibefore < iping) {
	  ping[iping].lon = ping[ibefore].lon;
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
	else if (iafter > iping && iafter < nBuff_ - 1) {
          dtime = ping[iafter + 1].time_d - ping[iafter].time_d;
	  if (dtime > 0.0)
	    ping[iping].lon = ping[iafter].lon + (ping[iafter + 1].lon - ping[iafter].lon) *
	      (ping[iping].time_d - ping[iafter].time_d) /
	      (ping[iafter + 1].time_d - ping[iafter].time_d);
          else
            ping[iping].lon = ping[iafter].lon;
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
	else if (iafter > iping) {
	  ping[iping].lon = ping[iafter].lon;
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
      }
    }

    /* do latitude */
    for (int iping = 0; iping < nBuff_; iping++) {
      if (ping[iping].lat_select) {
	int ibefore = iping;
	for (int i = iping - 1; i >= 0; i--) {
	  if (!ping[i].lat_select && ibefore == iping)
	    ibefore = i;
	}
	int iafter = iping;
	for (int i = iping + 1; i < nBuff_; i++) {
	  if (!ping[i].lat_select && iafter == iping)
	    iafter = i;
	}
	if (ibefore < iping && iafter > iping) {
	  dtime = ping[iafter].time_d - ping[ibefore].time_d;
          if (dtime > 0.0)
	    ping[iping].lat = ping[ibefore].lat + (ping[iafter].lat - ping[ibefore].lat) *
	      (ping[iping].time_d - ping[ibefore].time_d) /
	      (ping[iafter].time_d - ping[ibefore].time_d);
          else
	    ping[iping].lat = ping[ibefore].lat + 0.5 * (ping[iafter].lat - ping[ibefore].lat);
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
	else if (ibefore < iping && ibefore > 0) {
	  dtime = ping[iafter].time_d - ping[ibefore - 1].time_d;
	  if (dtime > 0.0)
	    ping[iping].lat = ping[ibefore].lat + (ping[ibefore].lat - ping[ibefore - 1].lat) *
	      (ping[iping].time_d - ping[ibefore].time_d) /
	      (ping[ibefore].time_d - ping[ibefore - 1].time_d);
          else
            ping[iping].lat = ping[ibefore].lat;
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
	else if (ibefore < iping) {
	  ping[iping].lat = ping[ibefore].lat;
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
	else if (iafter > iping && iafter < nBuff_ - 1) {
	  dtime = ping[iafter + 1].time_d - ping[iafter].time_d;
	  if (dtime > 0.0)
	    ping[iping].lat = ping[iafter].lat + (ping[iafter + 1].lat - ping[iafter].lat) *
	      (ping[iping].time_d - ping[iafter].time_d) /
	      (ping[iafter + 1].time_d - ping[iafter].time_d);
          else
            ping[iping].lat = ping[iafter].lat;
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
	else if (iafter > iping) {
	  ping[iping].lat = ping[iafter].lat;
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
      }
    }

    /* do speed */
    for (int iping = 0; iping < nBuff_; iping++) {
      if (ping[iping].speed_select) {
	int ibefore = iping;
	for (int i = iping - 1; i >= 0; i--) {
	  if (!ping[i].speed_select && ibefore == iping)
	    ibefore = i;
	}
	int iafter = iping;
	for (int i = iping + 1; i < nBuff_; i++) {
	  if (!ping[i].speed_select && iafter == iping)
	    iafter = i;
	}
	if (ibefore < iping && iafter > iping) {
	  dtime = ping[iafter].time_d - ping[ibefore].time_d;
	  if (dtime > 0.0)
	    ping[iping].speed = ping[ibefore].speed + (ping[iafter].speed - ping[ibefore].speed) *
	      (ping[iping].time_d - ping[ibefore].time_d) /
	      (ping[iafter].time_d - ping[ibefore].time_d);
          else
	    ping[iping].speed = ping[ibefore].speed + 0.5 * (ping[iafter].speed - ping[ibefore].speed);
	  speedheading_change = true;
	}
	else if (ibefore < iping) {
	  ping[iping].speed = ping[ibefore].speed;
	  speedheading_change = true;
	}
	else if (iafter > iping) {
	  ping[iping].speed = ping[iafter].speed;
	  speedheading_change = true;
	}
      }
    }

    /* do heading */
    for (int iping = 0; iping < nBuff_; iping++) {
      if (ping[iping].heading_select) {
	int ibefore = iping;
	for (int i = iping - 1; i >= 0; i--) {
	  if (!ping[i].heading_select && ibefore == iping)
	    ibefore = i;
	}
	int iafter = iping;
	for (int i = iping + 1; i < nBuff_; i++)
	  if (!ping[i].heading_select && iafter == iping)
	    iafter = i;
	if (ibefore < iping && iafter > iping) {
	  dtime = ping[iafter].time_d - ping[ibefore].time_d;
	  if (dtime > 0.0)
	    ping[iping].heading = ping[ibefore].heading + (ping[iafter].heading - ping[ibefore].heading) *
	      (ping[iping].time_d - ping[ibefore].time_d) /
	      (ping[iafter].time_d - ping[ibefore].time_d);
	  else
	    ping[iping].heading = ping[ibefore].heading + 0.5 * (ping[iafter].heading - ping[ibefore].heading);
	  speedheading_change = true;
	}
	else if (ibefore < iping) {
	  ping[iping].heading = ping[ibefore].heading;
	  speedheading_change = true;
	}
	else if (iafter > iping) {
	  ping[iping].heading = ping[iafter].heading;
	  speedheading_change = true;
	}
      }
    }

    /* do draft */
    for (int iping = 0; iping < nBuff_; iping++) {
      if (ping[iping].draft_select) {
	int ibefore = iping;
	for (int i = iping - 1; i >= 0; i--) {
	  if (!ping[i].draft_select && ibefore == iping)
	    ibefore = i;
	}
	int iafter = iping;
	for (int i = iping + 1; i < nBuff_; i++)
	  if (!ping[i].draft_select && iafter == iping)
	    iafter = i;
	if (ibefore < iping && iafter > iping) {
	  dtime = ping[iafter].time_d - ping[ibefore].time_d;
	  if (dtime > 0.0)
	    ping[iping].draft = ping[ibefore].draft + (ping[iafter].draft - ping[ibefore].draft) *
	      (ping[iping].time_d - ping[ibefore].time_d) /
	      (ping[iafter].time_d - ping[ibefore].time_d);
	  else
	    ping[iping].draft = ping[ibefore].draft + 0.5 * (ping[iafter].draft - ping[ibefore].draft);
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
	else if (ibefore < iping) {
	  ping[iping].draft = ping[ibefore].draft;
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
	else if (iafter > iping) {
	  ping[iping].draft = ping[iafter].draft;
	  ping[iping].lonlat_flag = true;
	  timelonlat_change = true;
	}
      }
    }

    /* recalculate speed-made-good and course-made-good */
    if (timelonlat_change)
      for (int i = 0; i < nBuff_; i++)
	get_smgcmg(i);

    /* recalculate model */
    if (speedheading_change && modelMode_ == MODEL_MODE_DR)
      get_model();
  }
  /* if no data then set failure flag */
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_interpolaterepeats(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  int status = MB_SUCCESS;
  // bool timelonlat_change;
  // bool speedheading_change;

  /* don't try to do anything if no data */
  if (nPlot_ > 0) {
    /* look for position or time changes */
    bool timelonlat_change = false;
    bool speedheading_change = false;
    int iafter;

    /* do expected time */
    for (int iping = 1; iping < nBuff_ - 1; iping++) {
      if (ping[iping].tint_select && ping[iping].time_d == ping[iping - 1].time_d) {
	/* find next changed value */
	bool found = false;
	int ibefore = iping - 1;
	for (int j = iping + 1; j < nBuff_ && !found; j++) {
	  if (ping[iping].time_d != ping[j].time_d) {
	    found = true;
	    iafter = j;
	  }
	}
	for (int j = iping; j < iafter; j++) {
	  if (ping[j].tint_select) {
	    ping[j].time_d = ping[ibefore].time_d + (ping[iafter].time_d - ping[ibefore].time_d) *
	      ((double)(iping - ibefore)) / ((double)(iafter - ibefore));
	    timelonlat_change = true;
	  }
	}
      }
    }

    /* do longitude */
    for (int iping = 1; iping < nBuff_ - 1; iping++) {
      if (ping[iping].lon_select && ping[iping].lon == ping[iping - 1].lon) {
	/* find next changed value */
	bool found = false;
	int ibefore = iping - 1;
	for (int j = iping + 1; j < nBuff_ && !found; j++) {
	  if (ping[iping].lon != ping[j].lon) {
	    found = true;
	    iafter = j;
	  }
	}
	for (int j = iping; j < iafter; j++) {
	  if (ping[j].lon_select) {
	    ping[j].lon = ping[ibefore].lon + (ping[iafter].lon - ping[ibefore].lon) *
	      (ping[j].time_d - ping[ibefore].time_d) /
	      (ping[iafter].time_d - ping[ibefore].time_d);
	    timelonlat_change = true;
	  }
	}
      }
    }

    /* do latitude */
    for (int iping = 1; iping < nBuff_ - 1; iping++) {
      if (ping[iping].lat_select && ping[iping].lat == ping[iping - 1].lat) {
	/* find next changed value */
	bool found = false;
	int ibefore = iping - 1;
	for (int j = iping + 1; j < nBuff_ && !found; j++) {
	  if (ping[iping].lat != ping[j].lat) {
	    found = true;
	    iafter = j;
	  }
	}
	for (int j = iping; j < iafter; j++) {
	  if (ping[j].lat_select) {
	    ping[j].lat = ping[ibefore].lat + (ping[iafter].lat - ping[ibefore].lat) *
	      (ping[j].time_d - ping[ibefore].time_d) /
	      (ping[iafter].time_d - ping[ibefore].time_d);
	    timelonlat_change = true;
	  }
	}
      }
    }

    /* do speed */
    for (int iping = 1; iping < nBuff_ - 1; iping++) {
      if (ping[iping].speed_select && ping[iping].speed == ping[iping - 1].speed) {
	/* find next changed value */
	bool found = false;
	int ibefore = iping - 1;
	for (int j = iping + 1; j < nBuff_ && !found; j++) {
	  if (ping[iping].speed != ping[j].speed) {
	    found = true;
	    iafter = j;
	  }
	}
	for (int j = iping; j < iafter; j++) {
	  if (ping[j].speed_select) {
	    ping[j].speed = ping[ibefore].speed + (ping[iafter].speed - ping[ibefore].speed) *
	      (ping[j].time_d - ping[ibefore].time_d) /
	      (ping[iafter].time_d - ping[ibefore].time_d);
	    speedheading_change = true;
	  }
	}
      }
    }

    /* do heading */
    for (int iping = 1; iping < nBuff_ - 1; iping++) {
      if (ping[iping].heading_select && ping[iping].heading == ping[iping - 1].heading) {
	/* find next changed value */
	bool found = false;
	int ibefore = iping - 1;
	for (int j = iping + 1; j < nBuff_ && !found; j++) {
	  if (ping[iping].heading != ping[j].heading) {
	    found = true;
	    iafter = j;
	  }
	}
	for (int j = iping; j < iafter; j++) {
	  if (ping[j].heading_select) {
	    ping[j].heading = ping[ibefore].heading + (ping[iafter].heading - ping[ibefore].heading) *
	      (ping[j].time_d - ping[ibefore].time_d) /
	      (ping[iafter].time_d - ping[ibefore].time_d);
	    speedheading_change = true;
	  }
	}
      }
    }

    /* do draft */
    for (int iping = 1; iping < nBuff_ - 1; iping++) {
      if (ping[iping].draft_select && ping[iping].draft == ping[iping - 1].draft) {
	/* find next changed value */
	bool found = false;
	int ibefore = iping - 1;
	for (int j = iping + 1; j < nBuff_ && !found; j++) {
	  if (ping[iping].draft != ping[j].draft) {
	    found = true;
	    iafter = j;
	  }
	}
	for (int j = iping; j < iafter; j++) {
	  if (ping[j].draft_select) {
	    ping[j].draft = ping[ibefore].draft + (ping[iafter].draft - ping[ibefore].draft) *
	      (ping[j].time_d - ping[ibefore].time_d) /
	      (ping[iafter].time_d - ping[ibefore].time_d);
	    timelonlat_change = true;
	  }
	}
      }
    }

    /* recalculate speed-made-good and course-made-good */
    if (timelonlat_change)
      for (int i = 0; i < nBuff_; i++)
	get_smgcmg(i);

    /* recalculate model */
    if (speedheading_change && modelMode_ == MODEL_MODE_DR)
      get_model();
  }
  /* if no data then set failure flag */
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_revert(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  int status = MB_SUCCESS;

  /* don't try to do anything if no data */
  if (nPlot_ > 0) {

    /* look for position changes */
    bool timelonlat_change = false;
    bool speedheading_change = false;

    /* loop over each of the plots */
    for (int iplot = 0; iplot < nPlots_; iplot++) {
      for (int i = currentId_; i < currentId_ + nPlot_; i++) {
	if (plot_[iplot].type == PLOT_TINT) {
	  if (ping[i].tint_select) {
	    ping[i].time_d = ping[i].time_d_org;
	    ping[i].file_time_d = ping[i].time_d - fileStarttime_d_;
	    ping[i].tint = ping[i].time_d - ping[i - 1].time_d;
	    timelonlat_change = true;
	    if (i < nBuff_ - 1)
	      ping[i + 1].tint = ping[i + 1].time_d - ping[i].time_d;
	    status = mb_get_date(verbose_, ping[i].time_d, ping[i].time_i);
	  }
	}
	else if (plot_[iplot].type == PLOT_LONGITUDE) {
	  if (ping[i].lon_select) {
	    ping[i].lon = ping[i].lon_org;
	    timelonlat_change = true;
	  }
	}
	else if (plot_[iplot].type == PLOT_LATITUDE) {
	  if (ping[i].lat_select) {
	    ping[i].lat = ping[i].lat_org;
	    timelonlat_change = true;
	  }
	}
	else if (plot_[iplot].type == PLOT_SPEED) {
	  if (ping[i].speed_select) {
	    ping[i].speed = ping[i].speed_org;
	    speedheading_change = true;
	  }
	}
	else if (plot_[iplot].type == PLOT_HEADING) {
	  if (ping[i].heading_select) {
	    ping[i].heading = ping[i].heading_org;
	    speedheading_change = true;
	  }
	}
	else if (plot_[iplot].type == PLOT_DRAFT) {
	  if (ping[i].draft_select) {
	    ping[i].draft = ping[i].draft_org;
	  }
	}
      }
    }

    /* recalculate speed-made-good and course-made-good */
    if (timelonlat_change)
      for (int i = 0; i < nBuff_; i++)
	get_smgcmg(i);

    /* recalculate model */
    if (speedheading_change && modelMode_ == MODEL_MODE_DR)
      get_model();

    /* clear the screen */
    status = clear_screen();

    /* replot the screen */
    status = plot_all();
  }
  /* if no data then set failure flag */
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_flag(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  int status = MB_SUCCESS;

  /* don't try to do anything if no data */
  if (nPlot_ > 0) {

    /* loop over each of the plots */
    for (int iplot = 0; iplot < nPlots_; iplot++) {
      for (int i = currentId_; i < currentId_ + nPlot_; i++) {
	if (plot_[iplot].type == PLOT_LONGITUDE) {
	  if (ping[i].lon_select) {
	    ping[i].lonlat_flag = true;
	  }
	}
	else if (plot_[iplot].type == PLOT_LATITUDE) {
	  if (ping[i].lat_select) {
	    ping[i].lonlat_flag = true;
	  }
	}
      }
    }

    /* clear the screen */
    status = clear_screen();

    /* replot the screen */
    status = plot_all();
  }
  /* if no data then set failure flag */
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_unflag(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  int status = MB_SUCCESS;

  /* don't try to do anything if no data */
  if (nPlot_ > 0) {

    /* loop over each of the plots */
    for (int iplot = 0; iplot < nPlots_; iplot++) {
      for (int i = currentId_; i < currentId_ + nPlot_; i++) {
	if (plot_[iplot].type == PLOT_LONGITUDE) {
	  if (ping[i].lon_select) {
	    ping[i].lonlat_flag = false;
	  }
	}
	else if (plot_[iplot].type == PLOT_LATITUDE) {
	  if (ping[i].lat_select) {
	    ping[i].lonlat_flag = false;
	  }
	}
      }
    }

    /* clear the screen */
    status = clear_screen();

    /* replot the screen */
    status = plot_all();
  }
  /* if no data then set failure flag */
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_fixtime(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  int istart, iend;
  double start_time_d, end_time_d;

  /* loop over the data */
  for (int i = 0; i < nBuff_; i++) {
    if (i == 0) {
      istart = i;
      start_time_d = ping[i].time_d;
    }
    else if (ping[i].time_d > start_time_d) {
      iend = i;
      end_time_d = ping[i].time_d;
      for (int j = istart + 1; j < iend; j++) {
	ping[j].time_d = start_time_d + (j - istart) * (end_time_d - start_time_d) / (iend - istart);
	mb_get_date(verbose_, ping[j].time_d, ping[j].time_i);
	ping[j].file_time_d = ping[j].time_d - fileStarttime_d_;
	if (j > 0)
	  ping[j - 1].tint = ping[j].time_d - ping[j - 1].time_d;
	if (j < nBuff_ - 1)
	  ping[j].tint = ping[j + 1].time_d - ping[j].time_d;
      }
      istart = i;
      start_time_d = ping[i].time_d;
    }
  }

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_deletebadtime(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  double lastgood_time_d;
  int nbuffnew;

  /* loop over the data looking for bad times */
  lastgood_time_d = ping[0].time_d;
  for (int i = 1; i < nBuff_; i++) {
    if ((ping[i].time_d - lastgood_time_d) <= 0.0) {
      ping[i].id = -1;
    }
    else if ((ping[i].time_d - lastgood_time_d) > 60.0) {
      if (i == nBuff_ - 1)
	ping[i].id = -1;
      else if (ping[i + 1].time_d - ping[i].time_d <= 0.0)
	ping[i].id = -1;
      else
	lastgood_time_d = ping[i].time_d;
    }
    else if (ping[i].time_d > ping[nBuff_ - 1].time_d) {
      ping[i].id = -1;
    }
    else {
      lastgood_time_d = ping[i].time_d;
    }
  }

  /* loop over the data in reverse deleting data with bad times */
  nbuffnew = nBuff_;
  for (int i = nBuff_ - 1; i >= 0; i--) {
    if (ping[i].id == -1) {
      for (int j = i; j < nbuffnew - 1; j++) {
	ping[j] = ping[j + 1];
      }
      if (i > 0)
	ping[i - 1].tint = ping[i].time_d - ping[i - 1].time_d;
      if (i == nbuffnew - 2 && i > 0)
	ping[i].tint = ping[i - 1].tint;
      else if (i == nbuffnew - 2 && i == 0)
	ping[i].tint = 0.0;
      nbuffnew--;
    }
  }
  fprintf(stderr, "Data deleted: nbuff:%d nbuffnew:%d\n", nBuff_, nbuffnew);
  nBuff_ = nbuffnew;

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::action_showall(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  /* reset plotting time span */
  if (nBuff_ > 0) {
    plotStartTime_ = ping[0].file_time_d;
    plotEndTime_ = ping[nBuff_ - 1].file_time_d;
    dataShowSize_ = 0;
    currentId_ = 0;
  }

  /* replot */
  const int status = plot_all();

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::get_smgcmg(int i) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       i:          %d\n", i);
  }

  int status = MB_SUCCESS;

  /* calculate speed made good and course made for ping i */
  if (i < nBuff_) {
    double time_d1, lon1, lat1;
    double time_d2, lon2, lat2;
    if (i == 0) {
      time_d1 = ping[i].time_d;
      lon1 = ping[i].lon;
      lat1 = ping[i].lat;
      time_d2 = ping[i + 1].time_d;
      lon2 = ping[i + 1].lon;
      lat2 = ping[i + 1].lat;
    }
    else if (i == nBuff_ - 1) {
      time_d1 = ping[i - 1].time_d;
      lon1 = ping[i - 1].lon;
      lat1 = ping[i - 1].lat;
      time_d2 = ping[i].time_d;
      lon2 = ping[i].lon;
      lat2 = ping[i].lat;
    }
    else {
      time_d1 = ping[i - 1].time_d;
      lon1 = ping[i - 1].lon;
      lat1 = ping[i - 1].lat;
      time_d2 = ping[i].time_d;
      lon2 = ping[i].lon;
      lat2 = ping[i].lat;
    }
    double mtodeglon;
    double mtodeglat;
    mb_coor_scale(verbose_, lat1, &mtodeglon, &mtodeglat);
    const double del_time = time_d2 - time_d1;
    const double dx = (lon2 - lon1) / mtodeglon;
    const double dy = (lat2 - lat1) / mtodeglat;
    const double dist = sqrt(dx * dx + dy * dy);
    if (del_time > 0.0)
      ping[i].speed_made_good = 3.6 * dist / del_time;
    else
      ping[i].speed_made_good = 0.0;
    if (dist > 0.0)
      ping[i].course_made_good = RTD * atan2(dx / dist, dy / dist);
    else
      ping[i].course_made_good = ping[i].heading;
    if (ping[i].course_made_good < 0.0)
      ping[i].course_made_good = ping[i].course_made_good + 360.0;

    status = MB_SUCCESS;
  }
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::get_model(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
  }

  /* only model if data available */
  if (nBuff_ > 0) {
    /* call correct modeling function */
    if (modelMode_ == MODEL_MODE_MEAN)
      get_gaussianmean();
    else if (modelMode_ == MODEL_MODE_DR)
      get_dr();
    else if (modelMode_ == MODEL_MODE_INVERT)
      get_inversion();
  }

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::get_gaussianmean(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
  }

  /* loop over navigation calculating gaussian mean positions */
  const double timewindow = 0.1 * meanTimeWindow_;
  const double a = -4.0 / (timewindow * timewindow);
  int jstart = 0;
  for (int i = 0; i < nBuff_; i++) {
    double dt = 0.0;
    double weight = 0.0;
    double sumlon = 0.0;
    double sumlat = 0.0;
    int nsum = 0;
    int npos = 0;
    int nneg = 0;
    for (int j = jstart; j < nBuff_ && dt <= timewindow; j++) {
      dt = ping[j].time_d - ping[i].time_d;
      if (!ping[j].lonlat_flag && fabs(dt) <= timewindow) {
	const double w = exp(a * dt * dt);
	nsum++;
	if (dt < 0.0)
	  nneg++;
	if (dt >= 0.0)
	  npos++;
	weight += w;
	sumlon += w * ping[j].lon;
	sumlat += w * ping[j].lat;
	if (nsum == 1)
	  jstart = j;
      }
    }
    if (npos > 0 && nneg > 0) {
      ping[i].mean_ok = true;
      ping[i].lon_dr = sumlon / weight;
      ping[i].lat_dr = sumlat / weight;
    }
    else {
      ping[i].mean_ok = false;
      ping[i].lon_dr = ping[i].lon;
      ping[i].lat_dr = ping[i].lat;
    }
  }

  /* loop over navigation performing linear interpolation to fill gaps */
  int jbefore = -1;
  for (int i = 0; i < nBuff_; i++) {
    /* only work on nav not smoothed in first past due to lack of nearby data */
    if (!ping[i].mean_ok) {
      /* find valid points before and after */
      int jafter = i;
      for (int j = jbefore; j < nBuff_ && jafter == i; j++) {
	if (j < i && !ping[j].lonlat_flag)
	  jbefore = j;
	if (j > i && !ping[j].lonlat_flag)
	  jafter = j;
      }
      if (jbefore >= 0 && jafter > i) {
	const double dt = (ping[i].time_d - ping[jbefore].time_d) / (ping[jafter].time_d - ping[jbefore].time_d);
	ping[i].lon_dr = ping[jbefore].lon + dt * (ping[jafter].lon - ping[jbefore].lon);
	ping[i].lat_dr = ping[jbefore].lat + dt * (ping[jafter].lat - ping[jbefore].lat);
      }
      else if (jbefore >= 0) {
	ping[i].lon_dr = ping[jbefore].lon;
	ping[i].lat_dr = ping[jbefore].lat;
      }
      else if (jafter > i) {
	ping[i].lon_dr = ping[jafter].lon;
	ping[i].lat_dr = ping[jafter].lat;
      }
      else {
	ping[i].lon_dr = ping[i].lon;
	ping[i].lat_dr = ping[i].lat;
      }
    }
  }

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::get_dr(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
  }

  double mtodeglon, mtodeglat;
  double del_time, dx, dy;
  double driftlon, driftlat;

  /* calculate dead reckoning */
  driftlon = 0.00001 * driftLon_;
  driftlat = 0.00001 * driftLat_;
  for (int i = 0; i < nBuff_; i++) {
    if (i == 0) {
      ping[i].lon_dr = ping[i].lon;
      ping[i].lat_dr = ping[i].lat;
    }
    else {
      del_time = ping[i].time_d - ping[i - 1].time_d;
      if (del_time < 300.0) {
	mb_coor_scale(verbose_, ping[i].lat, &mtodeglon, &mtodeglat);
	dx = sin(DTR * ping[i].heading) * ping[i].speed * del_time / 3.6;
	dy = cos(DTR * ping[i].heading) * ping[i].speed * del_time / 3.6;
	ping[i].lon_dr = ping[i - 1].lon_dr + dx * mtodeglon + del_time * driftlon / 3600.0;
	ping[i].lat_dr = ping[i - 1].lat_dr + dy * mtodeglat + del_time * driftlat / 3600.0;
      }
      else {
	ping[i].lon_dr = ping[i].lon;
	ping[i].lat_dr = ping[i].lat;
      }
    }
  }

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::get_inversion(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
  }

  /* set maximum dimensions of the inverse problem */
  const int nrows = nPlot_ + (nPlot_ - 1) + (nPlot_ - 2);
  const int ncols = nPlot_;
  const int nnz = 3;
  const int ncycle = 512;
  const double bandwidth = 10000.0;

  /* get average lon value */
  double lon_avg = 0.0;
  int nlon_avg = 0;
  double lat_avg = 0.0;
  int nlat_avg = 0;
  int first = currentId_;
  int last = currentId_;
  for (int i = currentId_; i < currentId_ + nPlot_; i++) {
    /* constrain lon unless flagged by user */
    if (!ping[i].lonlat_flag) {
      lon_avg += ping[i].lon;
      nlon_avg++;
      lat_avg += ping[i].lat;
      nlat_avg++;
      last = i;
    }
    else if (first == i && i < currentId_ + nPlot_ - 1) {
      first = i + 1;
    }
  }
  if (nlon_avg > 0)
    lon_avg /= nlon_avg;
  if (nlat_avg > 0)
    lat_avg /= nlat_avg;

  double mtodeglon;
  double mtodeglat;
  mb_coor_scale(verbose_, lat_avg, &mtodeglon, &mtodeglat);

  /* allocate space for the inverse problem */
  double *a;
  int status = mb_mallocd(verbose_, __FILE__, __LINE__, nnz * nrows * sizeof(double), (void **)&a, &error_);
  int *ia;
  status = mb_mallocd(verbose_, __FILE__, __LINE__, nnz * nrows * sizeof(int), (void **)&ia, &error_);
  int *nia;
  status = mb_mallocd(verbose_, __FILE__, __LINE__, nrows * sizeof(int), (void **)&nia, &error_);
  double *d;
  status = mb_mallocd(verbose_, __FILE__, __LINE__, nrows * sizeof(double), (void **)&d, &error_);
  double *x;
  status = mb_mallocd(verbose_, __FILE__, __LINE__, ncols * sizeof(double), (void **)&x, &error_);
  int *nx;
  status = mb_mallocd(verbose_, __FILE__, __LINE__, ncols * sizeof(int), (void **)&nx, &error_);
  double *dx;
  status = mb_mallocd(verbose_, __FILE__, __LINE__, ncols * sizeof(double), (void **)&dx, &error_);
  double *sigma;
  status = mb_mallocd(verbose_, __FILE__, __LINE__, ncycle * sizeof(double), (void **)&sigma, &error_);
  double *work;
  status = mb_mallocd(verbose_, __FILE__, __LINE__, ncycle * sizeof(double), (void **)&work, &error_);

  /* do inversion */
  if (error_ == MB_ERROR_NO_ERROR) {
    /* set message */
    char string[MB_PATH_MAXLINE];
    sprintf(string, "Setting up inversion of %d longitude points", nPlot_);
    showMessage(string);

    /* initialize arrays */
    for (int i = 0; i < nrows; i++) {
      nia[i] = 0;
      d[i] = 0.0;
      for (int j = 0; j < nnz; j++) {
	const int k = nnz * i + j;
	ia[k] = 0;
	a[k] = 0.0;
      }
    }
    for (int i = 0; i < ncols; i++) {
      nx[i] = 0;
      x[i] = 0;
      dx[i] = 0.0;
    }
    for (int i = 0; i < ncycle; i++) {
      sigma[i] = 0;
      work[i] = 0.0;
    }

    double dtime_d;
    double dtime_d_sq;

    /* loop over all nav points - add constraints for
       original lon values, speed, acceleration */
    int nr = 0;
    int nc = nPlot_;
    for (int i = currentId_; i < currentId_ + nPlot_; i++) {
      const int ii = i - currentId_;

      /* constrain lon unless flagged by user */
      if (!ping[i].lonlat_flag) {
	const int k = nnz * nr;
	d[nr] = (ping[i].lon_org - lon_avg) / mtodeglon;
	nia[nr] = 1;
	ia[k] = ii;
	a[k] = 1.0;
	nr++;
      }

      /* constrain speed */
      if (weightSpeed_ > 0.0 && ii > 0 && ping[i].time_d > ping[i - 1].time_d) {
	/* get time difference */
	dtime_d = ping[i].time_d - ping[i - 1].time_d;

	/* constrain lon speed */
	const int k = nnz * nr;
	d[nr] = 0.0;
	nia[nr] = 2;
	ia[k] = ii - 1;
	a[k] = -weightSpeed_ / dtime_d;
	ia[k + 1] = ii;
	a[k + 1] = weightSpeed_ / dtime_d;
	nr++;
      }

      /* constrain acceleration */
      if (weightAccel_ > 0.0 && ii > 0 && ii < nPlot_ - 1 && ping[i + 1].time_d > ping[i - 1].time_d) {
	/* get time difference */
	dtime_d = ping[i + 1].time_d - ping[i - 1].time_d;
	dtime_d_sq = dtime_d * dtime_d;

	/* constrain lon acceleration */
	const int k = nnz * nr;
	d[nr] = 0.0;
	nia[nr] = 3;
	ia[k] = ii - 1;
	a[k] = weightAccel_ / dtime_d_sq;
	ia[k + 1] = ii;
	a[k + 1] = -2.0 * weightAccel_ / dtime_d_sq;
	ia[k + 2] = ii + 1;
	a[k + 2] = weightAccel_ / dtime_d_sq;
	nr++;
      }
    }

    /* set message */
    sprintf(string, "Inverting %dX%d for smooth longitude...", nc, nr);
    showMessage(string);

    /* compute upper bound on maximum eigenvalue */
    int ncyc = 0;
    int nsig = 0;
    double smax;
    double sup;
    double err;
    lspeig(a, ia, nia, nnz, nc, nr, ncyc, &nsig, x, dx, sigma, work, &smax, &err, &sup);
    double supt = smax + err;
    if (sup > supt)
      supt = sup;
    if (verbose_ > 1)
      fprintf(stderr, "Initial lspeig: %g %g %g %g\n", sup, smax, err, supt);
    ncyc = 16;
    for (int i = 0; i < 4; i++) {
      lspeig(a, ia, nia, nnz, nc, nr, ncyc, &nsig, x, dx, sigma, work, &smax, &err, &sup);
      supt = smax + err;
      if (sup > supt)
	supt = sup;
      if (verbose_ > 1)
	fprintf(stderr, "lspeig[%d]: %g %g %g %g\n", i, sup, smax, err, supt);
    }

    /* calculate chebyshev factors (errlsq is the theoretical error_) */
    double slo = supt / bandwidth;
    chebyu(sigma, ncycle, supt, slo, work);
    double errlsq = errlim(sigma, ncycle, supt, slo);
    if (verbose_ > 1)
      fprintf(stderr, "Theoretical error: %f\n", errlsq);
    if (verbose_ > 1)
      for (int i = 0; i < ncycle; i++)
	fprintf(stderr, "sigma[%d]: %f\n", i, sigma[i]);

    /* solve the problem */
    for (int i = 0; i < nc; i++)
      x[i] = 0.0;
    lsqup(a, ia, nia, nnz, nc, nr, x, dx, d, 0, NULL, NULL, ncycle, sigma);

    /* generate solution */
    for (int i = currentId_; i < currentId_ + nPlot_; i++) {
      const int ii = i - currentId_;
      ping[i].lon_dr = lon_avg + mtodeglon * x[ii];
    }

    /* make flagged ends of data flat */
    for (int i = currentId_; i < first; i++) {
      const int ii = first - currentId_;
      ping[i].lon_dr = lon_avg + mtodeglon * x[ii];
    }
    for (int i = last + 1; i < currentId_ + nPlot_; i++) {
      const int ii = last - currentId_;
      ping[i].lon_dr = lon_avg + mtodeglon * x[ii];
    }

    /* set message */
    sprintf(string, "Setting up inversion of %d latitude points", nPlot_);
    showMessage(string);

    /* initialize arrays */
    for (int i = 0; i < nrows; i++) {
      nia[i] = 0;
      d[i] = 0.0;
      for (int j = 0; j < nnz; j++) {
	const int k = nnz * i + j;
	ia[k] = 0;
	a[k] = 0.0;
      }
    }
    for (int i = 0; i < ncols; i++) {
      nx[i] = 0;
      x[i] = 0;
      dx[i] = 0.0;
    }
    for (int i = 0; i < ncycle; i++) {
      sigma[i] = 0;
      work[i] = 0.0;
    }

    /* loop over all nav points - add constraints for
       original lat values, speed, acceleration */
    nr = 0;
    nc = nPlot_;
    for (int i = currentId_; i < currentId_ + nPlot_; i++) {
      const int ii = i - currentId_;

      /* constrain lat unless flagged by user */
      if (!ping[i].lonlat_flag) {
	const int k = nnz * nr;
	d[nr] = (ping[i].lat_org - lat_avg) / mtodeglat;
	nia[nr] = 1;
	ia[k] = ii;
	a[k] = 1.0;
	nr++;
      }

      /* constrain speed */
      if (weightSpeed_ > 0.0 && ii > 0 && ping[i].time_d > ping[i - 1].time_d) {
	/* get time difference */
	dtime_d = ping[i].time_d - ping[i - 1].time_d;

	/* constrain lat speed */
	const int k = nnz * nr;
	d[nr] = 0.0;
	nia[nr] = 2;
	ia[k] = ii - 1;
	a[k] = -weightSpeed_ / dtime_d;
	ia[k + 1] = ii;
	a[k + 1] = weightSpeed_ / dtime_d;
	nr++;
      }

      /* constrain acceleration */
      if (weightAccel_ > 0.0 && ii > 0 && ii < nPlot_ - 1 && ping[i + 1].time_d > ping[i - 1].time_d) {
	/* get time difference */
	dtime_d = ping[i + 1].time_d - ping[i - 1].time_d;
	dtime_d_sq = dtime_d * dtime_d;

	/* constrain lat acceleration */
	const int k = nnz * nr;
	d[nr] = 0.0;
	nia[nr] = 3;
	ia[k] = ii - 1;
	a[k] = weightAccel_ / dtime_d_sq;
	ia[k + 1] = ii;
	a[k + 1] = -2.0 * weightAccel_ / dtime_d_sq;
	ia[k + 2] = ii + 1;
	a[k + 2] = weightAccel_ / dtime_d_sq;
	nr++;
      }
    }

    /* set message */
    sprintf(string, "Inverting %dX%d for smooth latitude...", nc, nr);
    showMessage(string);

    /* compute upper bound on maximum eigenvalue */
    ncyc = 0;
    nsig = 0;
    lspeig(a, ia, nia, nnz, nc, nr, ncyc, &nsig, x, dx, sigma, work, &smax, &err, &sup);
    supt = smax + err;
    if (sup > supt)
      supt = sup;
    if (verbose_ > 1)
      fprintf(stderr, "Initial lspeig: %g %g %g %g\n", sup, smax, err, supt);
    ncyc = 16;
    for (int i = 0; i < 4; i++) {
      lspeig(a, ia, nia, nnz, nc, nr, ncyc, &nsig, x, dx, sigma, work, &smax, &err, &sup);
      supt = smax + err;
      if (sup > supt)
	supt = sup;
      if (verbose_ > 1)
	fprintf(stderr, "lspeig[%d]: %g %g %g %g\n", i, sup, smax, err, supt);
    }

    /* calculate chebyshev factors (errlsq is the theoretical error_) */
    slo = supt / bandwidth;
    chebyu(sigma, ncycle, supt, slo, work);
    errlsq = errlim(sigma, ncycle, supt, slo);
    if (verbose_ > 1)
      fprintf(stderr, "Theoretical error: %f\n", errlsq);
    if (verbose_ > 1)
      for (int i = 0; i < ncycle; i++)
	fprintf(stderr, "sigma[%d]: %f\n", i, sigma[i]);

    /* solve the problem */
    for (int i = 0; i < nc; i++)
      x[i] = 0.0;
    lsqup(a, ia, nia, nnz, nc, nr, x, dx, d, 0, NULL, NULL, ncycle, sigma);

    /* generate solution */
    for (int i = currentId_; i < currentId_ + nPlot_; i++) {
      const int ii = i - currentId_;
      ping[i].lat_dr = lat_avg + mtodeglat * x[ii];
    }

    /* make flagged ends of data flat */
    for (int i = currentId_; i < first; i++) {
      const int ii = first - currentId_;
      ping[i].lat_dr = lat_avg + mtodeglat * x[ii];
    }
    for (int i = last + 1; i < currentId_ + nPlot_; i++) {
      const int ii = last - currentId_;
      ping[i].lat_dr = lat_avg + mtodeglat * x[ii];
    }

    /* deallocate arrays */
    status = mb_freed(verbose_, __FILE__, __LINE__, (void **)&a, &error_);
    status = mb_freed(verbose_, __FILE__, __LINE__, (void **)&ia, &error_);
    status = mb_freed(verbose_, __FILE__, __LINE__, (void **)&nia, &error_);
    status = mb_freed(verbose_, __FILE__, __LINE__, (void **)&d, &error_);
    status = mb_freed(verbose_, __FILE__, __LINE__, (void **)&x, &error_);
    status = mb_freed(verbose_, __FILE__, __LINE__, (void **)&nx, &error_);
    status = mb_freed(verbose_, __FILE__, __LINE__, (void **)&dx, &error_);
    status = mb_freed(verbose_, __FILE__, __LINE__, (void **)&sigma, &error_);
    status = mb_freed(verbose_, __FILE__, __LINE__, (void **)&work, &error_);

    /* turn message off */
    hideMessage();
  }

  /* if error initializing memory then don't invert */
  else if (error_ != MB_ERROR_NO_ERROR) {
    mb_error(verbose_, error_, &message_);
    fprintf(stderr, "\nMBIO Error allocating data arrays:\n%s\n", message_);
    showError("Unable to invert for smooth",
	      "navigation due to a memory",
	      "allocation error!");
  }

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:       %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:      %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::plot_all(void) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
  }

  /* figure out which pings to plot */
  nPlot_ = 0;
  if (dataShowSize_ > 0 && nBuff_ > 0) {
    plotStartTime_ = ping[currentId_].file_time_d;
    plotEndTime_ = plotStartTime_ + dataShowSize_;
    for (int i = currentId_; i < nBuff_; i++)
      if (ping[i].file_time_d <= plotEndTime_)
	nPlot_++;
  } else if (nBuff_ > 0) {
    plotStartTime_ = ping[0].file_time_d;
    plotEndTime_ = ping[nBuff_ - 1].file_time_d;
    dataShowSize_ = plotEndTime_ - plotStartTime_ + 1;
    if (dataShowMax_ < dataShowSize_)
      dataShowMax_ = dataShowSize_;
    nPlot_ = nBuff_;
  }

  /* deselect data outside plots */
  for (int i = 0; i < currentId_; i++) {
    ping[i].tint_select = false;
    ping[i].lon_select = false;
    ping[i].lat_select = false;
    ping[i].speed_select = false;
    ping[i].heading_select = false;
    ping[i].draft_select = false;
  }
  for (int i = currentId_ + nPlot_; i < nBuff_; i++) {
    ping[i].tint_select = false;
    ping[i].lon_select = false;
    ping[i].lat_select = false;
    ping[i].speed_select = false;
    ping[i].heading_select = false;
    ping[i].draft_select = false;
  }

  /* don't try to plot if no data */
  int status = MB_SUCCESS;
  if (nPlot_ > 0) {
    /* find min max values */
    double time_min = plotStartTime_;
    double time_max = plotEndTime_;
    double tint_min = ping[currentId_].tint;
    double tint_max = ping[currentId_].tint;
    double lon_min = ping[currentId_].lon;
    double lon_max = ping[currentId_].lon;
    double lat_min = ping[currentId_].lat;
    double lat_max = ping[currentId_].lat;
    double speed_min = 0.0;
    double speed_max = ping[currentId_].speed;
    double heading_min = ping[currentId_].heading;
    double heading_max = ping[currentId_].heading;
    double draft_min = ping[currentId_].draft;
    double draft_max = ping[currentId_].draft;
    double roll_min = ping[currentId_].roll;
    double roll_max = ping[currentId_].roll;
    double pitch_min = ping[currentId_].pitch;
    double pitch_max = ping[currentId_].pitch;
    double heave_min = ping[currentId_].heave;
    double heave_max = ping[currentId_].heave;
    for (int i = currentId_ + 1; i < currentId_ + nPlot_; i++) {
      tint_min = MIN(ping[i].tint, tint_min);
      tint_max = MAX(ping[i].tint, tint_max);
      if (plotTintOrg_) {
	tint_min = MIN(ping[i].tint_org, tint_min);
	tint_max = MAX(ping[i].tint_org, tint_max);
      }
      lon_min = MIN(ping[i].lon, lon_min);
      lon_max = MAX(ping[i].lon, lon_max);
      if (plotLonOrg_) {
	lon_min = MIN(ping[i].lon_org, lon_min);
	lon_max = MAX(ping[i].lon_org, lon_max);
      }
      if (modelMode_ != MODEL_MODE_OFF && plotLonDr_) {
	lon_min = MIN(ping[i].lon_dr, lon_min);
	lon_max = MAX(ping[i].lon_dr, lon_max);
      }
      lat_min = MIN(ping[i].lat, lat_min);
      lat_max = MAX(ping[i].lat, lat_max);
      if (plotLatOrg_) {
	lat_min = MIN(ping[i].lat_org, lat_min);
	lat_max = MAX(ping[i].lat_org, lat_max);
      }
      if (modelMode_ != MODEL_MODE_OFF && plotLatDr_) {
	lat_min = MIN(ping[i].lat_dr, lat_min);
	lat_max = MAX(ping[i].lat_dr, lat_max);
      }
      speed_min = MIN(ping[i].speed, speed_min);
      speed_max = MAX(ping[i].speed, speed_max);
      if (plotSpeedOrg_) {
	speed_min = MIN(ping[i].speed_org, speed_min);
	speed_max = MAX(ping[i].speed_org, speed_max);
      }
      if (plotSmg_) {
	speed_min = MIN(ping[i].speed_made_good, speed_min);
	speed_max = MAX(ping[i].speed_made_good, speed_max);
      }
      heading_min = MIN(ping[i].heading, heading_min);
      heading_max = MAX(ping[i].heading, heading_max);
      if (plotHeadingOrg_) {
	heading_min = MIN(ping[i].heading_org, heading_min);
	heading_max = MAX(ping[i].heading_org, heading_max);
      }
      if (plotCmg_) {
	heading_min = MIN(ping[i].course_made_good, heading_min);
	heading_max = MAX(ping[i].course_made_good, heading_max);
      }
      draft_min = MIN(ping[i].draft, draft_min);
      draft_max = MAX(ping[i].draft, draft_max);
      if (plotDraftOrg_) {
	draft_min = MIN(ping[i].draft_org, draft_min);
	draft_max = MAX(ping[i].draft_org, draft_max);
      }
      roll_min = MIN(ping[i].roll, roll_min);
      roll_max = MAX(ping[i].roll, roll_max);
      pitch_min = MIN(ping[i].pitch, pitch_min);
      pitch_max = MAX(ping[i].pitch, pitch_max);
      heave_min = MIN(ping[i].heave, heave_min);
      heave_max = MAX(ping[i].heave, heave_max);
    }

    /* scale the min max a bit larger so all points fit on plots */
    double center = 0.5 * (time_min + time_max);
    double range = 0.51 * (time_max - time_min);
    time_min = center - range;
    time_max = center + range;
    center = 0.5 * (tint_min + tint_max);
    range = 0.55 * (tint_max - tint_min);
    tint_min = center - range;
    tint_max = center + range;
    center = 0.5 * (lon_min + lon_max);
    range = 0.55 * (lon_max - lon_min);
    lon_min = center - range;
    lon_max = center + range;
    center = 0.5 * (lat_min + lat_max);
    range = 0.55 * (lat_max - lat_min);
    lat_min = center - range;
    lat_max = center + range;
    if (speed_min < 0.0) {
      center = 0.5 * (speed_min + speed_max);
      range = 0.55 * (speed_max - speed_min);
      speed_min = center - range;
      speed_max = center + range;
    }
    else
      speed_max = 1.05 * speed_max;
    center = 0.5 * (heading_min + heading_max);
    range = 0.55 * (heading_max - heading_min);
    heading_min = center - range;
    heading_max = center + range;
    center = 0.5 * (draft_min + draft_max);
    range = 0.55 * (draft_max - draft_min);
    draft_min = center - range;
    draft_max = center + range;
    roll_max = 1.1 * MAX(fabs(roll_min), fabs(roll_max));
    roll_min = -roll_max;
    pitch_max = 1.1 * MAX(fabs(pitch_min), fabs(pitch_max));
    pitch_min = -pitch_max;
    heave_max = 1.1 * MAX(fabs(heave_min), fabs(heave_max));
    heave_min = -heave_max;

    /* make sure lon and lat scaled the same if both plotted */
    if (plotLon_ && plotLat_) {
      if ((lon_max - lon_min) > (lat_max - lat_min)) {
	center = 0.5 * (lat_min + lat_max);
	lat_min = center - 0.5 * (lon_max - lon_min);
	lat_max = center + 0.5 * (lon_max - lon_min);
      }
      else {
	center = 0.5 * (lon_min + lon_max);
	lon_min = center - 0.5 * (lat_max - lat_min);
	lon_max = center + 0.5 * (lat_max - lat_min);
      }
    }

    /* make sure min max values aren't too small */
    if ((tint_max - tint_min) < 0.01) {
      center = 0.5 * (tint_min + tint_max);
      tint_min = center - 0.005;
      tint_max = center + 0.005;
    }
    if ((lon_max - lon_min) < 0.001) {
      center = 0.5 * (lon_min + lon_max);
      lon_min = center - 0.0005;
      lon_max = center + 0.0005;
    }
    if ((lat_max - lat_min) < 0.001) {
      center = 0.5 * (lat_min + lat_max);
      lat_min = center - 0.0005;
      lat_max = center + 0.0005;
    }
    if (speed_max < 10.0)
      speed_max = 10.0;
    if ((heading_max - heading_min) < 10.0) {
      center = 0.5 * (heading_min + heading_max);
      heading_min = center - 5;
      heading_max = center + 5;
    }
    if ((draft_max - draft_min) < 0.1) {
      center = 0.5 * (draft_min + draft_max);
      draft_min = center - 0.05;
      draft_max = center + 0.05;
    }
    if ((roll_max - roll_min) < 2.0) {
      center = 0.5 * (roll_min + roll_max);
      roll_min = center - 1;
      roll_max = center + 1;
    }
    if ((pitch_max - pitch_min) < 2.0) {
      center = 0.5 * (pitch_min + pitch_max);
      pitch_min = center - 1;
      pitch_max = center + 1;
    }
    if ((heave_max - heave_min) < 0.02) {
      center = 0.5 * (heave_min + heave_max);
      heave_min = center - 0.01;
      heave_max = center + 0.01;
    }

    /* print out information */
    if (verbose_ >= 2) {
      fprintf(stderr, "\n%d data records set for plotting (%d desired)\n", nPlot_, dataShowSize_);
      for (int i = currentId_; i < currentId_ + nPlot_; i++)
	fprintf(stderr,
		"dbg5       %4d %4d %4d  %d/%d/%d %2.2d:%2.2d:%2.2d.%6.6d  %11.6f  %11.6f  %11.6f  %11.6f %11.6f %5.2f "
		"%5.1f %5.1f %5.1f %5.1f %5.1f\n",
		i, ping[i].id, ping[i].record, ping[i].time_i[1], ping[i].time_i[2], ping[i].time_i[0], ping[i].time_i[3],
		ping[i].time_i[4], ping[i].time_i[5], ping[i].time_i[6], ping[i].time_d, ping[i].file_time_d,
		ping[i].tint, ping[i].lon, ping[i].lat, ping[i].speed, ping[i].heading, ping[i].draft, ping[i].roll,
		ping[i].pitch, ping[i].heave);
    }

    /* get plot margins */
    const int margin_x = plotWidth_ / 10;
    const int margin_y = plotHeight_ / 6;

    /* get date at start of file */
    int xtime_i[7];
    mb_get_date(verbose_, fileStarttime_d_ + plotStartTime_, xtime_i);

    /* figure out how many plots to make */
    nPlots_ = 0;
    if (plotTint_) {
      plot_[nPlots_].type = PLOT_TINT;
      plot_[nPlots_].ixmin = 1.25 * margin_x;
      plot_[nPlots_].ixmax = plotWidth_ - margin_x / 2;
      plot_[nPlots_].iymin = plotHeight_ - margin_y + nPlots_ * plotHeight_;
      plot_[nPlots_].iymax = nPlots_ * plotHeight_ + margin_y;
      plot_[nPlots_].xmin = time_min;
      plot_[nPlots_].xmax = time_max;
      plot_[nPlots_].ymin = tint_min;
      plot_[nPlots_].ymax = tint_max;
      plot_[nPlots_].xscale = (plot_[nPlots_].ixmax - plot_[nPlots_].ixmin) /
	(plot_[nPlots_].xmax - plot_[nPlots_].xmin);
      plot_[nPlots_].yscale = (plot_[nPlots_].iymax - plot_[nPlots_].iymin) /
	(plot_[nPlots_].ymax - plot_[nPlots_].ymin);
      plot_[nPlots_].xinterval = 100.0;
      plot_[nPlots_].yinterval = 5.0;
      sprintf(plot_[nPlots_].xlabel, "Time (HH:MM:SS.SSS) beginning on %2.2d/%2.2d/%4.4d", xtime_i[1], xtime_i[2],
	      xtime_i[0]);
      sprintf(plot_[nPlots_].ylabel1, "dT");
      sprintf(plot_[nPlots_].ylabel2, "(seconds)");
      nPlots_++;
    }
    if (plotLon_) {
      plot_[nPlots_].type = PLOT_LONGITUDE;
      plot_[nPlots_].ixmin = 1.25 * margin_x;
      plot_[nPlots_].ixmax = plotWidth_ - margin_x / 2;
      plot_[nPlots_].iymin = plotHeight_ - margin_y + nPlots_ * plotHeight_;
      plot_[nPlots_].iymax = nPlots_ * plotHeight_ + margin_y;
      plot_[nPlots_].xmin = time_min;
      plot_[nPlots_].xmax = time_max;
      plot_[nPlots_].ymin = lon_min;
      plot_[nPlots_].ymax = lon_max;
      plot_[nPlots_].xscale = (plot_[nPlots_].ixmax - plot_[nPlots_].ixmin) /
	(plot_[nPlots_].xmax - plot_[nPlots_].xmin);
      plot_[nPlots_].yscale = (plot_[nPlots_].iymax - plot_[nPlots_].iymin) /
	(plot_[nPlots_].ymax - plot_[nPlots_].ymin);
      plot_[nPlots_].xinterval = 100.0;
      plot_[nPlots_].yinterval = 45.0;
      sprintf(plot_[nPlots_].xlabel, "Time (HH:MM:SS.SSS) beginning on %2.2d/%2.2d/%4.4d", xtime_i[1], xtime_i[2],
	      xtime_i[0]);
      sprintf(plot_[nPlots_].ylabel1, "Longitude");
      sprintf(plot_[nPlots_].ylabel2, "(degrees)");
      nPlots_++;
    }

    if (plotLat_) {
      plot_[nPlots_].type = PLOT_LATITUDE;
      plot_[nPlots_].ixmin = 1.25 * margin_x;
      plot_[nPlots_].ixmax = plotWidth_ - margin_x / 2;
      plot_[nPlots_].iymin = plotHeight_ - margin_y + nPlots_ * plotHeight_;
      plot_[nPlots_].iymax = nPlots_ * plotHeight_ + margin_y;
      plot_[nPlots_].xmin = time_min;
      plot_[nPlots_].xmax = time_max;
      plot_[nPlots_].ymin = lat_min;
      plot_[nPlots_].ymax = lat_max;
      plot_[nPlots_].xscale = (plot_[nPlots_].ixmax - plot_[nPlots_].ixmin) /
	(plot_[nPlots_].xmax - plot_[nPlots_].xmin);
      plot_[nPlots_].yscale = (plot_[nPlots_].iymax - plot_[nPlots_].iymin) /
	(plot_[nPlots_].ymax - plot_[nPlots_].ymin);
      plot_[nPlots_].xinterval = 100.0;
      plot_[nPlots_].yinterval = 45.0;
      sprintf(plot_[nPlots_].xlabel, "Time (HH:MM:SS.SSS) beginning on %2.2d/%2.2d/%4.4d", xtime_i[1], xtime_i[2],
	      xtime_i[0]);
      sprintf(plot_[nPlots_].ylabel1, "Latitude");
      sprintf(plot_[nPlots_].ylabel2, "(degrees)");
      nPlots_++;
    }
    if (plotSpeed_) {
      plot_[nPlots_].type = PLOT_SPEED;
      plot_[nPlots_].ixmin = 1.25 * margin_x;
      plot_[nPlots_].ixmax = plotWidth_ - margin_x / 2;
      plot_[nPlots_].iymin = plotHeight_ - margin_y + nPlots_ * plotHeight_;
      plot_[nPlots_].iymax = nPlots_ * plotHeight_ + margin_y;
      plot_[nPlots_].xmin = time_min;
      plot_[nPlots_].xmax = time_max;
      plot_[nPlots_].ymin = speed_min;
      plot_[nPlots_].ymax = speed_max;
      plot_[nPlots_].xscale = (plot_[nPlots_].ixmax - plot_[nPlots_].ixmin) /
	(plot_[nPlots_].xmax - plot_[nPlots_].xmin);
      plot_[nPlots_].yscale = (plot_[nPlots_].iymax - plot_[nPlots_].iymin) /
	(plot_[nPlots_].ymax - plot_[nPlots_].ymin);
      plot_[nPlots_].xinterval = 100.0;
      plot_[nPlots_].yinterval = 10;
      sprintf(plot_[nPlots_].xlabel, "Time (HH:MM:SS.SSS) beginning on %2.2d/%2.2d/%4.4d", xtime_i[1], xtime_i[2],
	      xtime_i[0]);
      sprintf(plot_[nPlots_].ylabel1, "Speed");
      sprintf(plot_[nPlots_].ylabel2, "(km/hr)");
      nPlots_++;
    }
    if (plotHeading_) {
      plot_[nPlots_].type = PLOT_HEADING;
      plot_[nPlots_].ixmin = 1.25 * margin_x;
      plot_[nPlots_].ixmax = plotWidth_ - margin_x / 2;
      plot_[nPlots_].iymin = plotHeight_ - margin_y + nPlots_ * plotHeight_;
      plot_[nPlots_].iymax = nPlots_ * plotHeight_ + margin_y;
      plot_[nPlots_].xmin = time_min;
      plot_[nPlots_].xmax = time_max;
      plot_[nPlots_].ymin = heading_min;
      plot_[nPlots_].ymax = heading_max;
      plot_[nPlots_].xscale = (plot_[nPlots_].ixmax - plot_[nPlots_].ixmin) /
	(plot_[nPlots_].xmax - plot_[nPlots_].xmin);
      plot_[nPlots_].yscale = (plot_[nPlots_].iymax - plot_[nPlots_].iymin) /
	(plot_[nPlots_].ymax - plot_[nPlots_].ymin);
      plot_[nPlots_].xinterval = 100.0;
      plot_[nPlots_].yinterval = 45.0;
      sprintf(plot_[nPlots_].xlabel, "Time (HH:MM:SS.SSS) beginning on %2.2d/%2.2d/%4.4d", xtime_i[1], xtime_i[2],
	      xtime_i[0]);
      sprintf(plot_[nPlots_].ylabel1, "Heading");
      sprintf(plot_[nPlots_].ylabel2, "(degrees)");
      nPlots_++;
    }
    if (plotDraft_) {
      plot_[nPlots_].type = PLOT_DRAFT;
      plot_[nPlots_].ixmin = 1.25 * margin_x;
      plot_[nPlots_].ixmax = plotWidth_ - margin_x / 2;
      plot_[nPlots_].iymin = plotHeight_ - margin_y + nPlots_ * plotHeight_;
      plot_[nPlots_].iymax = nPlots_ * plotHeight_ + margin_y;
      plot_[nPlots_].xmin = time_min;
      plot_[nPlots_].xmax = time_max;
      plot_[nPlots_].ymin = draft_max;
      plot_[nPlots_].ymax = draft_min;
      plot_[nPlots_].xscale = (plot_[nPlots_].ixmax - plot_[nPlots_].ixmin) /
	(plot_[nPlots_].xmax - plot_[nPlots_].xmin);
      plot_[nPlots_].yscale = (plot_[nPlots_].iymax - plot_[nPlots_].iymin) /
	(plot_[nPlots_].ymax - plot_[nPlots_].ymin);
      plot_[nPlots_].xinterval = 100.0;
      plot_[nPlots_].yinterval = 45.0;
      sprintf(plot_[nPlots_].xlabel, "Time (HH:MM:SS.SSS) beginning on %2.2d/%2.2d/%4.4d", xtime_i[1], xtime_i[2],
	      xtime_i[0]);
      sprintf(plot_[nPlots_].ylabel1, "Sonar Depth");
      sprintf(plot_[nPlots_].ylabel2, "(meters)");
      nPlots_++;
    }
    if (plotRoll_) {
      plot_[nPlots_].type = PLOT_ROLL;
      plot_[nPlots_].ixmin = 1.25 * margin_x;
      plot_[nPlots_].ixmax = plotWidth_ - margin_x / 2;
      plot_[nPlots_].iymin = plotHeight_ - margin_y + nPlots_ * plotHeight_;
      plot_[nPlots_].iymax = nPlots_ * plotHeight_ + margin_y;
      plot_[nPlots_].xmin = time_min;
      plot_[nPlots_].xmax = time_max;
      plot_[nPlots_].ymin = roll_min;
      plot_[nPlots_].ymax = roll_max;
      plot_[nPlots_].xscale = (plot_[nPlots_].ixmax - plot_[nPlots_].ixmin) /
	(plot_[nPlots_].xmax - plot_[nPlots_].xmin);
      plot_[nPlots_].yscale = (plot_[nPlots_].iymax - plot_[nPlots_].iymin) /
	(plot_[nPlots_].ymax - plot_[nPlots_].ymin);
      plot_[nPlots_].xinterval = 100.0;
      plot_[nPlots_].yinterval = 45.0;
      sprintf(plot_[nPlots_].xlabel, "Time (HH:MM:SS.SSS) beginning on %2.2d/%2.2d/%4.4d", xtime_i[1], xtime_i[2],
	      xtime_i[0]);
      sprintf(plot_[nPlots_].ylabel1, "Roll");
      sprintf(plot_[nPlots_].ylabel2, "(degrees)");
      nPlots_++;
    }
    if (plotPitch_) {
      plot_[nPlots_].type = PLOT_PITCH;
      plot_[nPlots_].ixmin = 1.25 * margin_x;
      plot_[nPlots_].ixmax = plotWidth_ - margin_x / 2;
      plot_[nPlots_].iymin = plotHeight_ - margin_y + nPlots_ * plotHeight_;
      plot_[nPlots_].iymax = nPlots_ * plotHeight_ + margin_y;
      plot_[nPlots_].xmin = time_min;
      plot_[nPlots_].xmax = time_max;
      plot_[nPlots_].ymin = pitch_min;
      plot_[nPlots_].ymax = pitch_max;
      plot_[nPlots_].xscale = (plot_[nPlots_].ixmax - plot_[nPlots_].ixmin) /
	(plot_[nPlots_].xmax - plot_[nPlots_].xmin);
      plot_[nPlots_].yscale = (plot_[nPlots_].iymax - plot_[nPlots_].iymin) /
	(plot_[nPlots_].ymax - plot_[nPlots_].ymin);
      plot_[nPlots_].xinterval = 100.0;
      plot_[nPlots_].yinterval = 45.0;
      sprintf(plot_[nPlots_].xlabel, "Time (HH:MM:SS.SSS) beginning on %2.2d/%2.2d/%4.4d", xtime_i[1], xtime_i[2],
	      xtime_i[0]);
      sprintf(plot_[nPlots_].ylabel1, "Pitch");
      sprintf(plot_[nPlots_].ylabel2, "(degrees)");
      nPlots_++;
    }
    if (plotHeave_) {
      plot_[nPlots_].type = PLOT_HEAVE;
      plot_[nPlots_].ixmin = 1.25 * margin_x;
      plot_[nPlots_].ixmax = plotWidth_ - margin_x / 2;
      plot_[nPlots_].iymin = plotHeight_ - margin_y + nPlots_ * plotHeight_;
      plot_[nPlots_].iymax = nPlots_ * plotHeight_ + margin_y;
      plot_[nPlots_].xmin = time_min;
      plot_[nPlots_].xmax = time_max;
      plot_[nPlots_].ymin = heave_min;
      plot_[nPlots_].ymax = heave_max;
      plot_[nPlots_].xscale = (plot_[nPlots_].ixmax - plot_[nPlots_].ixmin) /
	(plot_[nPlots_].xmax - plot_[nPlots_].xmin);
      plot_[nPlots_].yscale = (plot_[nPlots_].iymax - plot_[nPlots_].iymin) /
	(plot_[nPlots_].ymax - plot_[nPlots_].ymin);
      plot_[nPlots_].xinterval = 100.0;
      plot_[nPlots_].yinterval = 45.0;
      sprintf(plot_[nPlots_].xlabel, "Time (HH:MM:SS.SSS) beginning on %2.2d/%2.2d/%4.4d", xtime_i[1], xtime_i[2],
	      xtime_i[0]);
      sprintf(plot_[nPlots_].ylabel1, "Heave");
      sprintf(plot_[nPlots_].ylabel2, "(meters)");
      nPlots_++;
    }

    /* clear screen */
    status = clear_screen();

    /* do plots */
    for (int iplot = 0; iplot < nPlots_; iplot++) {
      /* get center locations */
      const int center_x = (plot_[iplot].ixmin + plot_[iplot].ixmax) / 2;
      const int center_y = (plot_[iplot].iymin + plot_[iplot].iymax) / 2;

      /* plot filename */
      char string[MB_PATH_MAXLINE+30];
      sprintf(string, "Data File: %s", ifile_);
      int swidth;
      int sascent;
      int sdescent;
      PixmapDrawer::justifyString(painter_, string, &swidth, &sascent, &sdescent);
      PixmapDrawer::drawString(painter_, center_x - swidth / 2,
			       plot_[iplot].iymax-5*sascent / 2,
			       string,
			       BLACK, SOLID_LINE);

      /* get bounds for position bar */
      int fpx = center_x - 2 * margin_x + (4 * margin_x * currentId_) / nBuff_;
      const int fpdx = MAX(((4 * margin_x * nPlot_) / nBuff_), 5);
      const int fpy = plot_[iplot].iymax - 2 * sascent;
      const int fpdy = sascent;
      if (fpdx > 4 * margin_x)
	fpx = center_x + 2 * margin_x - fpdx;

      /* plot file position bar */
      PixmapDrawer::drawRectangle(painter_,
				  center_x-2*margin_x, fpy,
				  4 * margin_x, fpdy,
				  BLACK, SOLID_LINE);
			
      PixmapDrawer::drawRectangle(painter_,
				  center_x - 2 * margin_x - 1,
				  fpy - 1, 4 * margin_x + 2, fpdy + 2,
				  BLACK, SOLID_LINE);
			
      PixmapDrawer::fillRectangle(painter_, fpx, fpy, fpdx, fpdy,
				  LIGHTGREY, SOLID_LINE);
			
      PixmapDrawer::drawRectangle(painter_, fpx, fpy, fpdx, fpdy,
				  BLACK, SOLID_LINE);

      sprintf(string, "0 ");
      PixmapDrawer::justifyString(painter_, string, &swidth,
				  &sascent, &sdescent);

      PixmapDrawer::drawString(painter_,
			       (int)(center_x - 2 * margin_x - swidth),
			       fpy + sascent, string,
			       BLACK,
			       SOLID_LINE);

      sprintf(string, " %d", nBuff_);
      PixmapDrawer::drawString(painter_,
			       (int)(center_x + 2 * margin_x),
			       fpy + sascent, string,
			       BLACK,
			       SOLID_LINE);

      /* plot x label */
      PixmapDrawer::justifyString(painter_, plot_[iplot].xlabel, &swidth, &sascent, &sdescent);
      PixmapDrawer::drawString(painter_,
			       (int)(center_x - swidth / 2),
			       (int)(plot_[iplot].iymin+
				     0.75*margin_y),
			       plot_[iplot].xlabel,
			       BLACK, SOLID_LINE);

      /* plot y labels */
      PixmapDrawer::justifyString(painter_, plot_[iplot].ylabel1, &swidth, &sascent, &sdescent);
      PixmapDrawer::drawString(painter_,
			       (int)(plot_[iplot].ixmin -
				     swidth / 2 - 0.75 * margin_x),
			       (int)(center_y - sascent),
			       plot_[iplot].ylabel1,
			       BLACK, SOLID_LINE);
			
      PixmapDrawer::justifyString(painter_, plot_[iplot].ylabel2, &swidth, &sascent, &sdescent);
      PixmapDrawer::drawString(painter_,
			       (int)(plot_[iplot].ixmin -
				     swidth / 2 - 0.75 * margin_x),
			       (int)(center_y + 2 * sascent),
			       plot_[iplot].ylabel2,
			       BLACK, SOLID_LINE);

      /* plot x axis time annotation */
      const double dx = (plotEndTime_ - plotStartTime_) / 5;
      for (int i = 0; i < 6; i++) {
	/* get x position */
	double x = plotStartTime_ + i * dx;
	const int ix = plot_[iplot].ixmin + plot_[iplot].xscale * (x - plot_[iplot].xmin);
	x += fileStarttime_d_;

	/* draw tickmarks */
	PixmapDrawer::drawLine(painter_, ix,
			       plot_[iplot].iymin, ix,
			       plot_[iplot].iymin + 5,
			       BLACK,
			       SOLID_LINE);

	/* draw annotations */
	mb_get_date(verbose_, x, xtime_i);
	sprintf(string, "%2.2d:%2.2d:%2.2d.%3.3d", xtime_i[3], xtime_i[4], xtime_i[5], (int)(0.001 * xtime_i[6]));
	PixmapDrawer::justifyString(painter_, string,
				    &swidth, &sascent, &sdescent);

	PixmapDrawer::drawString(painter_,
				 (int)(ix - swidth / 2),
				 (int)(plot_[iplot].iymin +
				       5 + 1.75 * sascent), string,
				 BLACK, SOLID_LINE);
      }

      /* plot y min max values */
      char yformat[10];
      if (plot_[iplot].type == PLOT_LONGITUDE || plot_[iplot].type == PLOT_LATITUDE)
	strcpy(yformat, "%11.6f");
      else
	strcpy(yformat, "%6.2f");
      sprintf(string, yformat, plot_[iplot].ymin);
      PixmapDrawer::justifyString(painter_, string, &swidth, &sascent, &sdescent);
      PixmapDrawer::drawString(painter_,
			       (int)(plot_[iplot].ixmin -
				     swidth - 0.03 * margin_x),
			       (int)(plot_[iplot].iymin + 0.5 *
				     sascent), string,
			       BLACK, SOLID_LINE);
			
      sprintf(string, yformat, plot_[iplot].ymax);
      PixmapDrawer::justifyString(painter_, string, &swidth,
				  &sascent, &sdescent);
			
      PixmapDrawer::drawString(painter_,
			       (int)(plot_[iplot].ixmin - swidth -
				     0.03 * margin_x),
			       (int)(plot_[iplot].iymax +
				     0.5 * sascent), string,
			       BLACK, SOLID_LINE);

      /* plot zero values */
      if ((plot_[iplot].ymax > 0.0 && plot_[iplot].ymin < 0.0) ||
	  (plot_[iplot].ymax < 0.0 && plot_[iplot].ymin > 0.0)) {
	if (plot_[iplot].type == PLOT_LONGITUDE || plot_[iplot].type == PLOT_LATITUDE)
	  strcpy(yformat, "%11.6f");
	else
	  strcpy(yformat, "%6.2f");
	sprintf(string, yformat, 0.0);
	PixmapDrawer::justifyString(painter_, string, &swidth, &sascent, &sdescent);
	const int iyzero = plot_[iplot].iymin - plot_[iplot].yscale * plot_[iplot].ymin;
	PixmapDrawer::drawString(painter_,
				 (int)(plot_[iplot].ixmin -
				       swidth - 0.03 * margin_x),
				 (int)(iyzero + 0.5 * sascent),
				 string, BLACK,
				 SOLID_LINE);
				
	PixmapDrawer::drawLine(painter_,
			       plot_[iplot].ixmin, iyzero,
			       plot_[iplot].ixmax, iyzero,
			       BLACK,
			       DASH_LINE);
      }

      /* plot bounding box */
      PixmapDrawer::drawRectangle(painter_,
				  plot_[iplot].ixmin,
				  plot_[iplot].iymax,
				  plot_[iplot].ixmax -
				  plot_[iplot].ixmin,
				  plot_[iplot].iymin -
				  plot_[iplot].iymax,
				  BLACK, SOLID_LINE);
			
      PixmapDrawer::drawRectangle(painter_,
				  plot_[iplot].ixmin - 1,
				  plot_[iplot].iymax - 1,
				  plot_[iplot].ixmax -
				  plot_[iplot].ixmin + 2,
				  plot_[iplot].iymin -
				  plot_[iplot].iymax + 2,
				  BLACK, SOLID_LINE);

      /* now plot the data */
      if (plot_[iplot].type == PLOT_TINT)
	plot_tint(iplot);
      else if (plot_[iplot].type == PLOT_LONGITUDE)
	plot_lon(iplot);
      else if (plot_[iplot].type == PLOT_LATITUDE)
	plot_lat(iplot);
      else if (plot_[iplot].type == PLOT_SPEED)
	plot_speed(iplot);
      else if (plot_[iplot].type == PLOT_HEADING)
	plot_heading(iplot);
      else if (plot_[iplot].type == PLOT_DRAFT)
	plot_draft(iplot);
      else if (plot_[iplot].type == PLOT_ROLL)
	plot_roll(iplot);
      else if (plot_[iplot].type == PLOT_PITCH)
	plot_pitch(iplot);
      else if (plot_[iplot].type == PLOT_HEAVE)
	plot_heave(iplot);
    }
  }

  /* set status */
  if (nPlot_ > 0)
    status = MB_SUCCESS;
  else
    status = MB_FAILURE;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::plot_tint(int iplot) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       iplot:       %d\n", iplot);
  }

  /* get scaling values */
  const int ixmin = plot_[iplot].ixmin;
  const int iymin = plot_[iplot].iymin;
  const double xmin = plot_[iplot].xmin;
  const double ymin = plot_[iplot].ymin;
  const double xscale = plot_[iplot].xscale;
  const double yscale = plot_[iplot].yscale;

  /* plot original expected time data */
  if (plotTintOrg_) {
    int tint_x1 = ixmin + xscale * (ping[currentId_].file_time_d - xmin);
    int tint_y1 = iymin + yscale * (ping[currentId_].tint_org - ymin);
    for (int i = currentId_ + 1; i < currentId_ + nPlot_; i++) {
      const int tint_x2 = ixmin + xscale * (ping[i].file_time_d - xmin);
      const int tint_y2 = iymin + yscale * (ping[i].tint_org - ymin);
      PixmapDrawer::drawLine(painter_, tint_x1, tint_y1,
			     tint_x2, tint_y2, GREEN,
			     SOLID_LINE);
			
      tint_x1 = tint_x2;
      tint_y1 = tint_y2;
    }
  }

  /* plot basic expected time data */
  for (int i = currentId_; i < currentId_ + nPlot_; i++) {
    ping[i].tint_x = ixmin + xscale * (ping[i].file_time_d - xmin);
    ping[i].tint_y = iymin + yscale * (ping[i].tint - ymin);
    if (ping[i].tint_select)
      PixmapDrawer::drawRectangle(painter_, ping[i].tint_x - 2,
				  ping[i].tint_y - 2, 4, 4,
				  RED, SOLID_LINE);
		
    else if (ping[i].tint != ping[i].tint_org)
      PixmapDrawer::drawRectangle(painter_, ping[i].tint_x - 2,
				  ping[i].tint_y - 2, 4, 4,
				  PURPLE, SOLID_LINE);
    else
      PixmapDrawer::fillRectangle(painter_, ping[i].tint_x - 2,
				  ping[i].tint_y - 2, 4, 4,
				  BLACK, SOLID_LINE);
  }

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::plot_lon(int iplot) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       iplot:       %d\n", iplot);
  }

  /* get scaling values */
  const int ixmin = plot_[iplot].ixmin;
  const int iymin = plot_[iplot].iymin;
  const double xmin = plot_[iplot].xmin;
  const double ymin = plot_[iplot].ymin;
  const double xscale = plot_[iplot].xscale;
  const double yscale = plot_[iplot].yscale;

  /* plot original longitude data */
  if (plotLonOrg_) {
    int lon_x1 = ixmin + xscale * (ping[currentId_].file_time_d - xmin);
    int lon_y1 = iymin + yscale * (ping[currentId_].lon_org - ymin);
    for (int i = currentId_ + 1; i < currentId_ + nPlot_; i++) {
      const int lon_x2 = ixmin + xscale * (ping[i].file_time_d - xmin);
      const int lon_y2 = iymin + yscale * (ping[i].lon_org - ymin);
      PixmapDrawer::drawLine(painter_, lon_x1, lon_y1,
			     lon_x2, lon_y2, GREEN,
			     SOLID_LINE);
      lon_x1 = lon_x2;
      lon_y1 = lon_y2;
    }
  }

  /* plot dr longitude data */
  if (modelMode_ != MODEL_MODE_OFF && plotLonDr_) {
    int lon_x1 = ixmin + xscale * (ping[currentId_].file_time_d - xmin);
    int lon_y1 = iymin + yscale * (ping[currentId_].lon_dr - ymin);
    for (int i = currentId_ + 1; i < currentId_ + nPlot_; i++) {
      const int lon_x2 = ixmin + xscale * (ping[i].file_time_d - xmin);
      const int lon_y2 = iymin + yscale * (ping[i].lon_dr - ymin);
      PixmapDrawer::drawLine(painter_, lon_x1, lon_y1,
			     lon_x2, lon_y2, BLUE,
			     SOLID_LINE);
      lon_x1 = lon_x2;
      lon_y1 = lon_y2;
    }
  }

  /* plot flagged longitude data first so it is overlain by all else */
  for (int i = currentId_; i < currentId_ + nPlot_; i++) {
    ping[i].lon_x = ixmin + xscale * (ping[i].file_time_d - xmin);
    ping[i].lon_y = iymin + yscale * (ping[i].lon - ymin);
    if (ping[i].lonlat_flag)
      PixmapDrawer::drawRectangle(painter_,
				  ping[i].lon_x - 2,
				  ping[i].lon_y - 2, 4, 4,
				  ORANGE, SOLID_LINE);
  }

  /* plot basic longitude data */
  for (int i = currentId_; i < currentId_ + nPlot_; i++) {
    ping[i].lon_x = ixmin + xscale * (ping[i].file_time_d - xmin);
    ping[i].lon_y = iymin + yscale * (ping[i].lon - ymin);
    if (ping[i].lon_select)
      PixmapDrawer::drawRectangle(painter_, ping[i].lon_x - 2,
				  ping[i].lon_y - 2, 4, 4,
				  RED, SOLID_LINE);
    else if (ping[i].lonlat_flag) {
      ;
    }
    else if (ping[i].lon != ping[i].lon_org)
      PixmapDrawer::drawRectangle(painter_, ping[i].lon_x - 2,
				  ping[i].lon_y - 2, 4, 4,
				  PURPLE, SOLID_LINE);
    else
      PixmapDrawer::fillRectangle(painter_, ping[i].lon_x - 2,
				  ping[i].lon_y - 2, 4, 4,
				  BLACK, SOLID_LINE);
  }

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::plot_lat(int iplot) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       iplot:       %d\n", iplot);
  }

  /* get scaling values */
  const int ixmin = plot_[iplot].ixmin;
  const int iymin = plot_[iplot].iymin;
  const double xmin = plot_[iplot].xmin;
  const double ymin = plot_[iplot].ymin;
  const double xscale = plot_[iplot].xscale;
  const double yscale = plot_[iplot].yscale;

  /* plot original latitude data */
  if (plotLatOrg_) {
    int lat_x1 = ixmin + xscale * (ping[currentId_].file_time_d - xmin);
    int lat_y1 = iymin + yscale * (ping[currentId_].lat_org - ymin);
    for (int i = currentId_ + 1; i < currentId_ + nPlot_; i++) {
      const int lat_x2 = ixmin + xscale * (ping[i].file_time_d - xmin);
      const int lat_y2 = iymin + yscale * (ping[i].lat_org - ymin);
      PixmapDrawer::drawLine(painter_, lat_x1, lat_y1, lat_x2,
			     lat_y2, GREEN, SOLID_LINE);
      lat_x1 = lat_x2;
      lat_y1 = lat_y2;
    }
  }

  /* plot dr latitude data */
  if (modelMode_ != MODEL_MODE_OFF && plotLatDr_) {
    int lat_x1 = ixmin + xscale * (ping[currentId_].file_time_d - xmin);
    int lat_y1 = iymin + yscale * (ping[currentId_].lat_dr - ymin);
    for (int i = currentId_ + 1; i < currentId_ + nPlot_; i++) {
      const int lat_x2 = ixmin + xscale * (ping[i].file_time_d - xmin);
      const int lat_y2 = iymin + yscale * (ping[i].lat_dr - ymin);
      PixmapDrawer::drawLine(painter_, lat_x1, lat_y1,
			     lat_x2, lat_y2, BLUE,
			     SOLID_LINE);
      lat_x1 = lat_x2;
      lat_y1 = lat_y2;
    }
  }

  /* plot flagged latitude data first so it is overlain by all else */
  for (int i = currentId_; i < currentId_ + nPlot_; i++) {
    ping[i].lat_x = ixmin + xscale * (ping[i].file_time_d - xmin);
    ping[i].lat_y = iymin + yscale * (ping[i].lat - ymin);
    if (ping[i].lonlat_flag)
      PixmapDrawer::drawRectangle(painter_, ping[i].lat_x - 2,
				  ping[i].lat_y - 2, 4, 4,
				  ORANGE, SOLID_LINE);
  }

  /* plot basic latitude data */
  for (int i = currentId_; i < currentId_ + nPlot_; i++) {
    ping[i].lat_x = ixmin + xscale * (ping[i].file_time_d - xmin);
    ping[i].lat_y = iymin + yscale * (ping[i].lat - ymin);
    if (ping[i].lat_select)
      PixmapDrawer::drawRectangle(painter_, ping[i].lat_x - 2,
				  ping[i].lat_y - 2, 4, 4,
				  RED, SOLID_LINE);
    else if (ping[i].lonlat_flag) {
      ;
    }
    else if (ping[i].lat != ping[i].lat_org)
      PixmapDrawer::drawRectangle(painter_, ping[i].lat_x - 2,
				  ping[i].lat_y - 2, 4, 4,
				  PURPLE, SOLID_LINE);
    else
      PixmapDrawer::fillRectangle(painter_, ping[i].lat_x - 2,
				  ping[i].lat_y - 2, 4, 4,
				  BLACK, SOLID_LINE);
  }

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::plot_speed(int iplot) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       iplot:       %d\n", iplot);
  }

  /* get scaling values */
  const int ixmin = plot_[iplot].ixmin;
  const int iymin = plot_[iplot].iymin;
  const double xmin = plot_[iplot].xmin;
  const double ymin = plot_[iplot].ymin;
  const double xscale = plot_[iplot].xscale;
  const double yscale = plot_[iplot].yscale;

  /* plot original speed data */
  if (plotSpeedOrg_) {
    int speed_x1 = ixmin + xscale * (ping[currentId_].file_time_d - xmin);
    int speed_y1 = iymin + yscale * (ping[currentId_].speed - ymin);
    for (int i = currentId_ + 1; i < currentId_ + nPlot_; i++) {
      const int speed_x2 = ixmin + xscale * (ping[i].file_time_d - xmin);
      const int speed_y2 = iymin + yscale * (ping[i].speed_org - ymin);
      PixmapDrawer::drawLine(painter_, speed_x1, speed_y1,
			     speed_x2, speed_y2, GREEN,
			     SOLID_LINE);
      speed_x1 = speed_x2;
      speed_y1 = speed_y2;
    }
  }

  /* plot speed made good data */
  if (plotSmg_) {
    int speed_x1 = ixmin + xscale * (ping[currentId_].file_time_d - xmin);
    int speed_y1 = iymin + yscale * (ping[currentId_].speed_made_good - ymin);
    for (int i = currentId_ + 1; i < currentId_ + nPlot_; i++) {
      const int speed_x2 = ixmin + xscale * (ping[i].file_time_d - xmin);
      const int speed_y2 = iymin + yscale * (ping[i].speed_made_good - ymin);
      PixmapDrawer::drawLine(painter_, speed_x1, speed_y1,
			     speed_x2, speed_y2, BLUE, SOLID_LINE);
      speed_x1 = speed_x2;
      speed_y1 = speed_y2;
    }
  }

  /* plot basic speed data */
  for (int i = currentId_; i < currentId_ + nPlot_; i++) {
    ping[i].speed_x = ixmin + xscale * (ping[i].file_time_d - xmin);
    ping[i].speed_y = iymin + yscale * (ping[i].speed - ymin);
    if (ping[i].speed_select)
      PixmapDrawer::drawRectangle(painter_, ping[i].speed_x - 2,
				  ping[i].speed_y - 2, 4, 4,
				  RED, SOLID_LINE);
    else if (ping[i].speed != ping[i].speed_org)
      PixmapDrawer::drawRectangle(painter_, ping[i].speed_x - 2,
				  ping[i].speed_y - 2, 4, 4,
				  PURPLE, SOLID_LINE);
    else
      PixmapDrawer::fillRectangle(painter_, ping[i].speed_x - 2,
				  ping[i].speed_y - 2, 4, 4,
				  BLACK, SOLID_LINE);
  }

  int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::plot_heading(int iplot) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       iplot:       %d\n", iplot);
  }

  /* get scaling values */
  const int ixmin = plot_[iplot].ixmin;
  const int iymin = plot_[iplot].iymin;
  const double xmin = plot_[iplot].xmin;
  const double ymin = plot_[iplot].ymin;
  const double xscale = plot_[iplot].xscale;
  const double yscale = plot_[iplot].yscale;

  /* plot original heading data */
  if (plotHeadingOrg_) {
    int heading_x1 = ixmin + xscale * (ping[currentId_].file_time_d - xmin);
    int heading_y1 = iymin + yscale * (ping[currentId_].heading - ymin);
    for (int i = currentId_ + 1; i < currentId_ + nPlot_; i++) {
      const int heading_x2 = ixmin + xscale * (ping[i].file_time_d - xmin);
      const int heading_y2 = iymin + yscale * (ping[i].heading_org - ymin);
      PixmapDrawer::drawLine(painter_, heading_x1, heading_y1,
			     heading_x2, heading_y2,
			     GREEN, SOLID_LINE);
      heading_x1 = heading_x2;
      heading_y1 = heading_y2;
    }
  }

  /* plot course made good data */
  if (plotCmg_) {
    int heading_x1 = ixmin + xscale * (ping[currentId_].file_time_d - xmin);
    int heading_y1 = iymin + yscale * (ping[currentId_].course_made_good - ymin);
    for (int i = currentId_ + 1; i < currentId_ + nPlot_; i++) {
      const int heading_x2 = ixmin + xscale * (ping[i].file_time_d - xmin);
      const int heading_y2 = iymin + yscale * (ping[i].course_made_good - ymin);
      PixmapDrawer::drawLine(painter_, heading_x1, heading_y1,
			     heading_x2, heading_y2,
			     BLUE, SOLID_LINE);
      heading_x1 = heading_x2;
      heading_y1 = heading_y2;
    }
  }

  /* plot basic heading data */
  for (int i = currentId_; i < currentId_ + nPlot_; i++) {
    ping[i].heading_x = ixmin + xscale * (ping[i].file_time_d - xmin);
    ping[i].heading_y = iymin + yscale * (ping[i].heading - ymin);
    if (ping[i].heading_select)
      PixmapDrawer::drawRectangle(painter_, ping[i].heading_x - 2,
				  ping[i].heading_y - 2, 4, 4,
				  RED, SOLID_LINE);
    else if (ping[i].heading != ping[i].heading_org)
      PixmapDrawer::drawRectangle(painter_, ping[i].heading_x - 2,
				  ping[i].heading_y - 2, 4, 4,
				  PURPLE,
				  SOLID_LINE);
    else
      PixmapDrawer::fillRectangle(painter_, ping[i].heading_x - 2,
				  ping[i].heading_y - 2, 4, 4,
				  BLACK,
				  SOLID_LINE);
  }

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::plot_draft(int iplot) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       iplot:       %d\n", iplot);
  }

  /* get scaling values */
  const int ixmin = plot_[iplot].ixmin;
  const int iymin = plot_[iplot].iymin;
  const double xmin = plot_[iplot].xmin;
  const double ymin = plot_[iplot].ymin;
  const double xscale = plot_[iplot].xscale;
  const double yscale = plot_[iplot].yscale;

  /* plot original draft data */
  if (plotDraftOrg_) {
    int draft_x1 = ixmin + xscale * (ping[currentId_].file_time_d - xmin);
    int draft_y1 = iymin + yscale * (ping[currentId_].draft - ymin);
    for (int i = currentId_ + 1; i < currentId_ + nPlot_; i++) {
      const int draft_x2 = ixmin + xscale * (ping[i].file_time_d - xmin);
      const int draft_y2 = iymin + yscale * (ping[i].draft_org - ymin);
      PixmapDrawer::drawLine(painter_, draft_x1, draft_y1, draft_x2, draft_y2,
			     GREEN, SOLID_LINE);
      draft_x1 = draft_x2;
      draft_y1 = draft_y2;
    }
  }

  /* plot basic draft data */
  for (int i = currentId_; i < currentId_ + nPlot_; i++) {
    ping[i].draft_x = ixmin + xscale * (ping[i].file_time_d - xmin);
    ping[i].draft_y = iymin + yscale * (ping[i].draft - ymin);
    if (ping[i].draft_select)
      PixmapDrawer::drawRectangle(painter_, ping[i].draft_x - 2, ping[i].draft_y - 2,
				  4, 4, RED, SOLID_LINE);
    else if (ping[i].draft != ping[i].draft_org)
      PixmapDrawer::drawRectangle(painter_, ping[i].draft_x - 2,
				  ping[i].draft_y - 2,
				  4, 4, PURPLE, SOLID_LINE);
    else
      PixmapDrawer::fillRectangle(painter_, ping[i].draft_x - 2,
				  ping[i].draft_y - 2, 4, 4, BLACK,
				  SOLID_LINE);
  }

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::plot_roll(int iplot) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       iplot:       %d\n", iplot);
  }

  /* plot roll data */
  if (plotRoll_) {
    /* get scaling values */
    const int ixmin = plot_[iplot].ixmin;
    const int iymin = plot_[iplot].iymin;
    const double xmin = plot_[iplot].xmin;
    const double ymin = plot_[iplot].ymin;
    const double xscale = plot_[iplot].xscale;
    const double yscale = plot_[iplot].yscale;

    int roll_x1 = ixmin + xscale * (ping[currentId_].file_time_d - xmin);
    int roll_y1 = iymin + yscale * (ping[currentId_].roll - ymin);
    for (int i = currentId_ + 1; i < currentId_ + nPlot_; i++) {
      const int roll_x2 = ixmin + xscale * (ping[i].file_time_d - xmin);
      const int roll_y2 = iymin + yscale * (ping[i].roll - ymin);
      PixmapDrawer::drawLine(painter_, roll_x1, roll_y1, roll_x2, roll_y2,
			     GREEN, SOLID_LINE);
      roll_x1 = roll_x2;
      roll_y1 = roll_y2;
    }
  }

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::plot_pitch(int iplot) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       iplot:       %d\n", iplot);
  }

  /* plot pitch data */
  if (plotPitch_) {
    /* get scaling values */
    const int ixmin = plot_[iplot].ixmin;
    const int iymin = plot_[iplot].iymin;
    const double xmin = plot_[iplot].xmin;
    const double ymin = plot_[iplot].ymin;
    const double xscale = plot_[iplot].xscale;
    const double yscale = plot_[iplot].yscale;

    int pitch_x1 = ixmin + xscale * (ping[currentId_].file_time_d - xmin);
    int pitch_y1 = iymin + yscale * (ping[currentId_].pitch - ymin);
    for (int i = currentId_ + 1; i < currentId_ + nPlot_; i++) {
      const int pitch_x2 = ixmin + xscale * (ping[i].file_time_d - xmin);
      const int pitch_y2 = iymin + yscale * (ping[i].pitch - ymin);
      PixmapDrawer::drawLine(painter_, pitch_x1, pitch_y1, pitch_x2, pitch_y2,
			     GREEN, SOLID_LINE);
      pitch_x1 = pitch_x2;
      pitch_y1 = pitch_y2;
    }
  }

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::plot_heave(int iplot) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       iplot:       %d\n", iplot);
  }

  /* plot heave data */
  if (plotHeave_) {
    /* get scaling values */
    const int ixmin = plot_[iplot].ixmin;
    const int iymin = plot_[iplot].iymin;
    const double xmin = plot_[iplot].xmin;
    const double ymin = plot_[iplot].ymin;
    const double xscale = plot_[iplot].xscale;
    const double yscale = plot_[iplot].yscale;

    int heave_x1 = ixmin + xscale * (ping[currentId_].file_time_d - xmin);
    int heave_y1 = iymin + yscale * (ping[currentId_].heave - ymin);
    for (int i = currentId_ + 1; i < currentId_ + nPlot_; i++) {
      const int heave_x2 = ixmin + xscale * (ping[i].file_time_d - xmin);
      const int heave_y2 = iymin + yscale * (ping[i].heave - ymin);
      PixmapDrawer::drawLine(painter_, heave_x1, heave_y1, heave_x2, heave_y2,
			     GREEN, SOLID_LINE);
      heave_x1 = heave_x2;
      heave_y1 = heave_y2;
    }
  }

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::plot_tint_value(int iplot, int iping) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       iplot:       %d\n", iplot);
    fprintf(stderr, "dbg2       iping:       %d\n", iping);
  }

  /* unplot basic expected time data value */
  PixmapDrawer::drawRectangle(painter_, ping[iping].tint_x - 2,
			      ping[iping].tint_y - 2, 4, 4, WHITE,
			      SOLID_LINE);
  PixmapDrawer::fillRectangle(painter_, ping[iping].tint_x - 2,
			      ping[iping].tint_y - 2, 4, 4, WHITE,
			      SOLID_LINE);

  /* replot basic expected time data value */
  if (ping[iping].tint_select)
    PixmapDrawer::drawRectangle(painter_, ping[iping].tint_x - 2,
				ping[iping].tint_y - 2, 4, 4, RED,
				SOLID_LINE);
  else if (ping[iping].tint != ping[iping].tint_org)
    PixmapDrawer::drawRectangle(painter_, ping[iping].tint_x - 2,
				ping[iping].tint_y - 2, 4, 4, PURPLE,
				SOLID_LINE);
  else
    PixmapDrawer::fillRectangle(painter_, ping[iping].tint_x - 2,
				ping[iping].tint_y - 2, 4, 4, BLACK,
				SOLID_LINE);

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::plot_lon_value(int iplot, int iping) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       iplot:       %d\n", iplot);
    fprintf(stderr, "dbg2       iping:       %d\n", iping);
  }

  /* unplot basic lon data value */
  PixmapDrawer::drawRectangle(painter_, ping[iping].lon_x - 2,
			      ping[iping].lon_y - 2, 4, 4, WHITE,
			      SOLID_LINE);
  PixmapDrawer::fillRectangle(painter_, ping[iping].lon_x - 2,
			      ping[iping].lon_y - 2, 4, 4, WHITE,
			      SOLID_LINE);

  /* replot basic lon data value */
  if (ping[iping].lon_select)
    PixmapDrawer::drawRectangle(painter_, ping[iping].lon_x - 2,
				ping[iping].lon_y - 2, 4, 4, RED,
				SOLID_LINE);
  else if (ping[iping].lonlat_flag)
    PixmapDrawer::drawRectangle(painter_, ping[iping].lon_x - 2,
				ping[iping].lon_y - 2, 4, 4, ORANGE,
				SOLID_LINE);
  else if (ping[iping].lon != ping[iping].lon_org)
    PixmapDrawer::drawRectangle(painter_, ping[iping].lon_x - 2,
				ping[iping].lon_y - 2, 4, 4, PURPLE,
				SOLID_LINE);
  else
    PixmapDrawer::fillRectangle(painter_, ping[iping].lon_x - 2,
				ping[iping].lon_y - 2, 4, 4, BLACK,
				SOLID_LINE);

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::plot_lat_value(int iplot, int iping) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       iplot:       %d\n", iplot);
    fprintf(stderr, "dbg2       iping:       %d\n", iping);
  }

  /* unplot basic lat data value */
  PixmapDrawer::drawRectangle(painter_, ping[iping].lat_x - 2,
			      ping[iping].lat_y - 2, 4, 4, WHITE,
			      SOLID_LINE);
  PixmapDrawer::fillRectangle(painter_, ping[iping].lat_x - 2,
			      ping[iping].lat_y - 2, 4, 4, WHITE,
			      SOLID_LINE);

  /* replot basic lat data value */
  if (ping[iping].lat_select)
    PixmapDrawer::drawRectangle(painter_, ping[iping].lat_x - 2,
				ping[iping].lat_y - 2, 4, 4, RED,
				SOLID_LINE);
  else if (ping[iping].lonlat_flag)
    PixmapDrawer::drawRectangle(painter_, ping[iping].lat_x - 2,
				ping[iping].lat_y - 2, 4, 4, ORANGE,
				SOLID_LINE);
  else if (ping[iping].lat != ping[iping].lat_org)
    PixmapDrawer::drawRectangle(painter_, ping[iping].lat_x - 2,
				ping[iping].lat_y - 2, 4, 4, PURPLE,
				SOLID_LINE);
  else
    PixmapDrawer::fillRectangle(painter_, ping[iping].lat_x - 2,
				ping[iping].lat_y - 2, 4, 4, BLACK,
				SOLID_LINE);

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::plot_speed_value(int iplot, int iping) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       iplot:       %d\n", iplot);
    fprintf(stderr, "dbg2       iping:       %d\n", iping);
  }

  /* unplot basic speed data value */
  PixmapDrawer::drawRectangle(painter_, ping[iping].speed_x - 2,
			      ping[iping].speed_y - 2, 4, 4, WHITE,
			      SOLID_LINE);
  PixmapDrawer::fillRectangle(painter_, ping[iping].speed_x - 2,
			      ping[iping].speed_y - 2, 4, 4, WHITE,
			      SOLID_LINE);

  /* replot basic speed data value */
  if (ping[iping].speed_select)
    PixmapDrawer::drawRectangle(painter_, ping[iping].speed_x - 2,
				ping[iping].speed_y - 2, 4, 4, RED,
				SOLID_LINE);
  else if (ping[iping].speed != ping[iping].speed_org)
    PixmapDrawer::drawRectangle(painter_, ping[iping].speed_x - 2,
				ping[iping].speed_y - 2, 4, 4, PURPLE,
				SOLID_LINE);
  else
    PixmapDrawer::fillRectangle(painter_, ping[iping].speed_x - 2,
				ping[iping].speed_y - 2, 4, 4, BLACK,
				SOLID_LINE);

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::plot_heading_value(int iplot, int iping) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       iplot:       %d\n", iplot);
    fprintf(stderr, "dbg2       iping:       %d\n", iping);
  }

  /* unplot basic heading data value */
  PixmapDrawer::drawRectangle(painter_, ping[iping].heading_x - 2,
			      ping[iping].heading_y - 2, 4, 4, WHITE,
			      SOLID_LINE);
  PixmapDrawer::fillRectangle(painter_, ping[iping].heading_x - 2,
			      ping[iping].heading_y - 2, 4, 4, WHITE,
			      SOLID_LINE);

  /* replot basic heading data value */
  if (ping[iping].heading_select)
    PixmapDrawer::drawRectangle(painter_, ping[iping].heading_x - 2,
				ping[iping].heading_y - 2, 4, 4, RED,
				SOLID_LINE);
  else if (ping[iping].heading != ping[iping].heading_org)
    PixmapDrawer::drawRectangle(painter_, ping[iping].heading_x - 2,
				ping[iping].heading_y - 2, 4, 4, PURPLE,
				SOLID_LINE);
  else
    PixmapDrawer::fillRectangle(painter_, ping[iping].heading_x - 2,
				ping[iping].heading_y - 2, 4, 4, BLACK,
				SOLID_LINE);

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/
int Backend::plot_draft_value(int iplot, int iping) {
  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> called\n", __func__);
    fprintf(stderr, "dbg2  Input arguments:\n");
    fprintf(stderr, "dbg2       iplot:       %d\n", iplot);
    fprintf(stderr, "dbg2       iping:       %d\n", iping);
  }

  /* unplot basic draft data value */
  PixmapDrawer::drawRectangle(painter_, ping[iping].draft_x - 2,
			      ping[iping].draft_y - 2, 4, 4, WHITE,
			      SOLID_LINE);
  PixmapDrawer::fillRectangle(painter_, ping[iping].draft_x - 2,
			      ping[iping].draft_y - 2, 4, 4, WHITE,
			      SOLID_LINE);

  /* replot basic draft data value */
  if (ping[iping].draft_select)
    PixmapDrawer::drawRectangle(painter_, ping[iping].draft_x - 2,
				ping[iping].draft_y - 2, 4, 4, RED,
				SOLID_LINE);
  else if (ping[iping].draft != ping[iping].draft_org)
    PixmapDrawer::drawRectangle(painter_, ping[iping].draft_x - 2,
				ping[iping].draft_y - 2, 4, 4, PURPLE,
				SOLID_LINE);
  else
    PixmapDrawer::fillRectangle(painter_, ping[iping].draft_x - 2,
				ping[iping].draft_y - 2, 4, 4, BLACK,
				SOLID_LINE);

  const int status = MB_SUCCESS;

  if (verbose_ >= 2) {
    fprintf(stderr, "\ndbg2  MBIO function <%s> completed\n", __func__);
    fprintf(stderr, "dbg2  Return values:\n");
    fprintf(stderr, "dbg2       error:      %d\n", error_);
    fprintf(stderr, "dbg2  Return status:\n");
    fprintf(stderr, "dbg2       status:  %d\n", status);
  }

  return (status);
}
/*--------------------------------------------------------------------*/


void Backend::parseInputDataList(char *file, int dummy) {
  std::cout << "Backend::parseInputDataList() not implemented\n";
}


int Backend::showError(const char *s1, const char *s2, const char *s3) {
    std::cerr << "showError(): " << s1 << "\n" << s2 << "\n" << s3 << "\n";
    char msg[256];
    sprintf(msg, "%s\n%s\n%s\n", s1, s2, s3);
    emit emitter_.showMessage(QVariant(msg));
    return 0;
  }


int Backend::showMessage(const char *message) {
  std::cerr << "showMessage(): " << message << "\n";
  emit emitter_.showMessage(QVariant(message));
    
  return 0;
}

