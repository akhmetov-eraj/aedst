{{/* Chart name, truncated to the 63-char DNS label limit. */}}
{{- define "aedst-server.name" -}}
{{- default .Chart.Name .Values.nameOverride | trunc 63 | trimSuffix "-" -}}
{{- end -}}

{{/* Fully qualified app name. */}}
{{- define "aedst-server.fullname" -}}
{{- if .Values.fullnameOverride -}}
{{- .Values.fullnameOverride | trunc 63 | trimSuffix "-" -}}
{{- else -}}
{{- $name := default .Chart.Name .Values.nameOverride -}}
{{- if contains $name .Release.Name -}}
{{- .Release.Name | trunc 63 | trimSuffix "-" -}}
{{- else -}}
{{- printf "%s-%s" .Release.Name $name | trunc 63 | trimSuffix "-" -}}
{{- end -}}
{{- end -}}
{{- end -}}

{{- define "aedst-server.chart" -}}
{{- printf "%s-%s" .Chart.Name .Chart.Version | replace "+" "_" | trunc 63 | trimSuffix "-" -}}
{{- end -}}

{{- define "aedst-server.selectorLabels" -}}
app.kubernetes.io/name: {{ include "aedst-server.name" . }}
app.kubernetes.io/instance: {{ .Release.Name }}
{{- end -}}

{{- define "aedst-server.labels" -}}
helm.sh/chart: {{ include "aedst-server.chart" . }}
{{ include "aedst-server.selectorLabels" . }}
app.kubernetes.io/version: {{ .Values.image.tag | default .Chart.AppVersion | quote }}
app.kubernetes.io/managed-by: {{ .Release.Service }}
{{- end -}}

{{- define "aedst-server.serviceAccountName" -}}
{{- if .Values.serviceAccount.create -}}
{{- default (include "aedst-server.fullname" .) .Values.serviceAccount.name -}}
{{- else -}}
{{- default "default" .Values.serviceAccount.name -}}
{{- end -}}
{{- end -}}

{{/* Name of the pre-existing Secret holding all sensitive values. */}}
{{- define "aedst-server.secretName" -}}
{{- required "existingSecret is required: name of an existing Secret with the database DSN and JWT key" .Values.existingSecret -}}
{{- end -}}

{{- define "aedst-server.image" -}}
{{- printf "%s:%s" .Values.image.repository (.Values.image.tag | default .Chart.AppVersion) -}}
{{- end -}}

{{- define "aedst-server.migrationImage" -}}
{{- printf "%s:%s" .Values.migrations.image.repository (.Values.migrations.image.tag | default .Chart.AppVersion) -}}
{{- end -}}

{{/*
terminationGracePeriodSeconds must exceed AEDST_SHUTDOWN_DRAIN_SECONDS (readiness
fails, listener still open) + AEDST_SHUTDOWN_TIMEOUT_SECONDS (in-flight requests
finish), otherwise the kubelet SIGKILLs the pod mid-drain.
*/}}
{{- define "aedst-server.terminationGracePeriodSeconds" -}}
{{- $min := add (int .Values.config.shutdownDrainSeconds) (int .Values.config.shutdownTimeoutSeconds) -}}
{{- if .Values.terminationGracePeriodSeconds -}}
{{- if le (int .Values.terminationGracePeriodSeconds) (int $min) -}}
{{- fail (printf "terminationGracePeriodSeconds (%v) must be greater than shutdownDrainSeconds + shutdownTimeoutSeconds (%v)" .Values.terminationGracePeriodSeconds $min) -}}
{{- end -}}
{{- int .Values.terminationGracePeriodSeconds -}}
{{- else -}}
{{- add $min 10 -}}
{{- end -}}
{{- end -}}

{{/* AEDST_ROLE_POLICY: procedure=role|role,procedure=role (map keys sorted by Helm). */}}
{{- define "aedst-server.rolePolicy" -}}
{{- $parts := list -}}
{{- range $procedure, $roles := .Values.config.rolePolicy -}}
{{- $parts = append $parts (printf "%s=%s" $procedure (join "|" $roles)) -}}
{{- end -}}
{{- join "," $parts -}}
{{- end }}
