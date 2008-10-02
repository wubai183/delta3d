#ifndef externaltool_h__
#define externaltool_h__

#include <QtCore/QObject>
class QAction;
class QProcess;

namespace dtEditQt 
{
   class ExternalTool : public QObject
   {
      Q_OBJECT
   public:
      ExternalTool();
      ~ExternalTool();
      void SetTitle(const QString& title);
      QString GetTitle() const;

      void SetCmd(const QString& command);
      const QString& GetCmd() const;

      QAction* GetAction() const;
   
   protected slots:

         void OnStartTool();

   private:
      QAction* mAction;
      QString mCommand;
      QProcess* mProcess;
   };
}

#endif // externaltool_h__

