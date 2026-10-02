import type { JSX } from 'react';

import type { QrPayload, WifiSecurity } from '@/types/qr';
import { Button } from '@/components/ui/Button';
import { Field } from '@/components/ui/Field';
import { Segmented } from '@/components/ui/Segmented';
import { Toggle } from '@/components/ui/Toggle';
import ui from '@/components/ui/ui.module.css';

export type SpecialKind = 'wifi' | 'vCard' | 'email' | 'phone' | 'sms' | 'geo';

const KINDS: readonly { value: SpecialKind; label: string }[] = [
  { value: 'wifi', label: 'Wi-Fi' },
  { value: 'vCard', label: 'Contact' },
  { value: 'email', label: 'Email' },
  { value: 'phone', label: 'Phone' },
  { value: 'sms', label: 'Message' },
  { value: 'geo', label: 'Place' },
];

interface PayloadFormsProps {
  payload: QrPayload;
  onChange: (payload: QrPayload) => void;
}

/**
 * Forms for the payload types that need more than one field.
 *
 * The automatic detector covers plain text, URLs, email addresses and phone numbers.
 * Everything here is opt-in: the user picks a type, fills the form, and the QR updates
 * as they type, exactly like the single input field does.
 */
export function PayloadForms({ payload, onChange }: PayloadFormsProps): JSX.Element {
  // Only `structured` mode renders this component, so the payload is always one
  // of the kinds below; text and URL are handled by the smart input instead.
  const kind = payload.type as SpecialKind;

  return (
    <div className={ui.stack}>
      <div className={ui.field}>
        <span className={ui.fieldLabel}>Type</span>
        <Segmented
          label="Payload type"
          value={kind}
          onChange={(next) => onChange(convert(next))}
          options={KINDS}
          fullWidth
        />
      </div>

      {payload.type === 'wifi' ? <WifiForm payload={payload} onChange={onChange} /> : null}
      {payload.type === 'vCard' ? <VCardForm payload={payload} onChange={onChange} /> : null}
      {payload.type === 'email' ? <EmailForm payload={payload} onChange={onChange} /> : null}
      {payload.type === 'phone' ? <PhoneForm payload={payload} onChange={onChange} /> : null}
      {payload.type === 'sms' ? <SmsForm payload={payload} onChange={onChange} /> : null}
      {payload.type === 'geo' ? <GeoForm payload={payload} onChange={onChange} /> : null}
    </div>
  );
}

/* ------------------------------------------------------------------- Wi-Fi */

function WifiForm({
  payload,
  onChange,
}: {
  payload: Extract<QrPayload, { type: 'wifi' }>;
  onChange: (payload: QrPayload) => void;
}): JSX.Element {
  const patch = (next: Partial<Extract<QrPayload, { type: 'wifi' }>>): void =>
    onChange({ ...payload, ...next });

  return (
    <>
      <Field label="Network name (SSID)">
        {({ id, describedBy }) => (
          <input
            className={ui.input}
            id={id}
            aria-describedby={describedBy}
            value={payload.ssid}
            autoComplete="off"
            onChange={(event) => patch({ ssid: event.target.value })}
          />
        )}
      </Field>

      <Field label="Security">
        {({ id, describedBy }) => (
          <select
            className={ui.select}
            id={id}
            aria-describedby={describedBy}
            value={payload.security}
            onChange={(event) => patch({ security: event.target.value as WifiSecurity })}
          >
            <option value="wpa">WPA / WPA2 / WPA3</option>
            <option value="wep">WEP</option>
            <option value="none">None (open)</option>
          </select>
        )}
      </Field>

      {payload.security !== 'none' ? (
        <Field label="Password">
          {({ id, describedBy }) => (
            <input
              className={ui.input}
              id={id}
              type="password"
              aria-describedby={describedBy}
              value={payload.password}
              autoComplete="off"
              onChange={(event) => patch({ password: event.target.value })}
            />
          )}
        </Field>
      ) : null}

      <Toggle
        label="Hidden network"
        description="The network does not broadcast its name."
        checked={payload.hidden}
        onChange={(hidden) => patch({ hidden })}
      />
    </>
  );
}

/* ------------------------------------------------------------------ vCard */

const VCARD_FIELDS = [
  { key: 'firstName', label: 'First name' },
  { key: 'lastName', label: 'Last name' },
  { key: 'organization', label: 'Organisation' },
  { key: 'jobTitle', label: 'Job title' },
  { key: 'phone', label: 'Phone' },
  { key: 'email', label: 'Email' },
  { key: 'website', label: 'Website' },
  { key: 'address', label: 'Address' },
  { key: 'note', label: 'Note' },
] as const;

function VCardForm({
  payload,
  onChange,
}: {
  payload: Extract<QrPayload, { type: 'vCard' }>;
  onChange: (payload: QrPayload) => void;
}): JSX.Element {
  return (
    <>
      {VCARD_FIELDS.map((field) => (
        <Field key={field.key} label={field.label}>
          {({ id, describedBy }) => (
            <input
              className={ui.input}
              id={id}
              aria-describedby={describedBy}
              value={payload[field.key]}
              autoComplete="off"
              onChange={(event) => onChange({ ...payload, [field.key]: event.target.value })}
            />
          )}
        </Field>
      ))}
    </>
  );
}

/* ------------------------------------------------------------------ email */

function EmailForm({
  payload,
  onChange,
}: {
  payload: Extract<QrPayload, { type: 'email' }>;
  onChange: (payload: QrPayload) => void;
}): JSX.Element {
  return (
    <>
      <Field label="To" hint="Separate several recipients with a comma.">
        {({ id, describedBy }) => (
          <input
            className={ui.input}
            id={id}
            aria-describedby={describedBy}
            value={payload.to}
            autoComplete="off"
            onChange={(event) => onChange({ ...payload, to: event.target.value })}
          />
        )}
      </Field>
      <Field label="Subject">
        {({ id, describedBy }) => (
          <input
            className={ui.input}
            id={id}
            aria-describedby={describedBy}
            value={payload.subject}
            onChange={(event) => onChange({ ...payload, subject: event.target.value })}
          />
        )}
      </Field>
      <Field label="Body">
        {({ id, describedBy }) => (
          <textarea
            className={ui.textarea}
            id={id}
            aria-describedby={describedBy}
            value={payload.body}
            rows={3}
            onChange={(event) => onChange({ ...payload, body: event.target.value })}
          />
        )}
      </Field>
    </>
  );
}

/* ------------------------------------------------------------------ phone */

function PhoneForm({
  payload,
  onChange,
}: {
  payload: Extract<QrPayload, { type: 'phone' }>;
  onChange: (payload: QrPayload) => void;
}): JSX.Element {
  return (
    <Field label="Phone number" hint="Formatting characters are removed automatically.">
      {({ id, describedBy }) => (
        <input
          className={ui.input}
          id={id}
          aria-describedby={describedBy}
          value={payload.number}
          autoComplete="off"
          onChange={(event) => onChange({ ...payload, number: event.target.value })}
        />
      )}
    </Field>
  );
}

/* -------------------------------------------------------------------- sms */

function SmsForm({
  payload,
  onChange,
}: {
  payload: Extract<QrPayload, { type: 'sms' }>;
  onChange: (payload: QrPayload) => void;
}): JSX.Element {
  return (
    <>
      <Field label="Number">
        {({ id, describedBy }) => (
          <input
            className={ui.input}
            id={id}
            aria-describedby={describedBy}
            value={payload.number}
            autoComplete="off"
            onChange={(event) => onChange({ ...payload, number: event.target.value })}
          />
        )}
      </Field>
      <Field label="Message">
        {({ id, describedBy }) => (
          <textarea
            className={ui.textarea}
            id={id}
            aria-describedby={describedBy}
            value={payload.message}
            rows={3}
            onChange={(event) => onChange({ ...payload, message: event.target.value })}
          />
        )}
      </Field>
    </>
  );
}

/* -------------------------------------------------------------------- geo */

function GeoForm({
  payload,
  onChange,
}: {
  payload: Extract<QrPayload, { type: 'geo' }>;
  onChange: (payload: QrPayload) => void;
}): JSX.Element {
  const patch = (next: Partial<Extract<QrPayload, { type: 'geo' }>>): void =>
    onChange({ ...payload, ...next });

  return (
    <>
      <div className={ui.grid}>
        <Field label="Latitude" hint="-90 to 90">
          {({ id, describedBy }) => (
            <input
              className={ui.input}
              id={id}
              type="number"
              step="any"
              aria-describedby={describedBy}
              value={payload.latitude}
              onChange={(event) => patch({ latitude: Number(event.target.value) })}
            />
          )}
        </Field>
        <Field label="Longitude" hint="-180 to 180">
          {({ id, describedBy }) => (
            <input
              className={ui.input}
              id={id}
              type="number"
              step="any"
              aria-describedby={describedBy}
              value={payload.longitude}
              onChange={(event) => patch({ longitude: Number(event.target.value) })}
            />
          )}
        </Field>
      </div>
      <Field label="Label" hint="Shown by the maps app when the code is scanned.">
        {({ id, describedBy }) => (
          <input
            className={ui.input}
            id={id}
            aria-describedby={describedBy}
            value={payload.label}
            autoComplete="off"
            onChange={(event) => patch({ label: event.target.value })}
          />
        )}
      </Field>
      <Button
        onClick={() => {
          navigator.geolocation?.getCurrentPosition((position) => {
            patch({
              latitude: position.coords.latitude,
              longitude: position.coords.longitude,
            });
          });
        }}
      >
        Use my location
      </Button>
    </>
  );
}

/* ------------------------------------------------------------- conversion */

const SECURITY_BY_KIND: WifiSecurity = 'wpa';

/** A fresh, empty payload of the chosen kind. */
function convert(kind: SpecialKind): QrPayload {
  switch (kind) {
    case 'wifi':
      return { type: 'wifi', ssid: '', password: '', security: SECURITY_BY_KIND, hidden: false };
    case 'vCard':
      return {
        type: 'vCard',
        firstName: '',
        lastName: '',
        organization: '',
        jobTitle: '',
        phone: '',
        email: '',
        website: '',
        address: '',
        note: '',
      };
    case 'email':
      return { type: 'email', to: '', subject: '', body: '' };
    case 'phone':
      return { type: 'phone', number: '' };
    case 'sms':
      return { type: 'sms', number: '', message: '' };
    case 'geo':
      return { type: 'geo', latitude: 0, longitude: 0, label: '' };
    default:
      return { type: 'text', text: '' };
  }
}
